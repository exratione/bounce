<?php


// TODO rip out the imap function implementations and read from the fake data
  

/**
 * @file
 * A test class for faking a connection to a mail server account via the IMAP package. It makes no pretensions to being
 * comprehensive or a good test for general use in connecting to a mailbox: it just fakes the small slice of IMAP package 
 * functionality that is important to this module.
 */

/**
 * A test class that fakes connections to a mail server via the IMAP package, allowing as much module functionality 
 * as possible to be exercised without having to set up an actual mail server.
 */
class BounceTestMailServerConnector extends BounceMailServerConnector {
  
  /**
   * Stores simulated error messages for imap_last_error().
   */
  public $last_error_message = FALSE;
  
  // various boolean switches for simulating failure of IMAP package functions
  public $fail_imap_close = FALSE;
  public $fail_imap_open = FALSE;
  public $fail_imap_timeout = FALSE;
  
  public function __construct($params) {
    parent::__construct($params);
  }
  
  //----------------------------------------------------------------------------
  // Mail templates
  //---------------------------------------------------------------------------- 
  
  /**
   * The upper level overview data for a mail.
   */
  public $mail_overview_generic = array(
  
    // Overview fields
    'subject' => 'the messages subject - irrelevant here since it is not used for analysis',
    'from' => 'mail-daemon@example.com',
    'to' => 'bounce@mydomain.com',
    'date' => NULL,
    'deleted' => FALSE,
    
    // Overview fields that can or will be set later
    /*
    'msgno'       - message sequence number in the mailbox
    */
  
    // Overview fields we don't use but which can exist in a real mailbox
    /*
    'message_id' - Message-ID
    'references' - is a reference to this message id
    'in_reply_to' - is a reply to this message id
    'size' - size in bytes
    'uid' - UID the message has in the mailbox
    'recent' - this message is flagged as recent
    'flagged' - this message is flagged
    'answered' - this message is flagged as answered
    'seen' - this message is flagged as already read
    'draft' - this message is flagged as being a draft
    */
  
    // Fields that don't exist in a real mailbox, but are here holding data
    'parts' => NULL
  );
  
  /**
   * Generate a data structure for a non-delivery report. The text will be
   * non-real, but sufficient to exercise module code.
   * 
   * @param string $smtp_code the code that should be extracted from the mail
   * @param string $recipient_email_address the email address of the original recipient
   * @return array a mail data structure for the fake mailbox
   */
  public function __generateSMTPNonDeliveryReport($smtp_code, $recipient_email_address) {
    $parts = array();
    // headers are largely irrelevant for our purposes, so use dummy values
    $headers_part = array(
      'data' => '',
      'charset' => 'utf-8',
    );
    $headers = array(
      'header1' => 'value1',
      'header2' => 'value2',
    );
    foreach ($headers as $name => $value) {
      $headers_part['data'] .= $name . ': ' . $value . "\n";
    }
    $parts[] = $headers_part;
    // add two sections to the body
    $parts[] = array(
      'data' => 'blah blah ' . $smtp_code . ' blah blah',
      'charset' => 'utf-8',
    );
    $parts[] = array(
      'data' => 'blah blah ' . $recipient_email_address . ' blah blah',
      'charset' => 'utf-8',
    );
    
    $report = $this->mail_overview_generic;
    $report['parts'] = $parts;
    return $report;
  }

  
  //----------------------------------------------------------------------------
  // Methods for managing test mail data.
  //---------------------------------------------------------------------------- 
  
  /**
   * Return the mailbox.
   */
  public function __load_mailbox() {
    return variable_get('bounce_test_connector_mailbox', array());
  }
  
  /**
   * Store the updated mailbox.
   * 
   * @param array $mailbox
   */
  public function __store_mailbox($mailbox) {
    variable_set('bounce_test_connector_mailbox', $mailbox);
  }
  
  /**
   * Reset the mailbox for a new test run.
   */
  public function __reset_mailbox() {
    variable_set('bounce_test_connector_mailbox', array());
  } 
  
  /**
   * Remove all messages from the fake mailbox.
   */
  public function __removeMessagesFlaggedForDeletion() {
    $mailbox = $this->__load_mailbox();
    foreach ($mailbox as $index => $mail) {
      if ($mail['deleted']) {
        unset($mailbox[$index]);
      }
    }
    
    // and reorder the numbering on the mailbox
    $ordered_mailbox = array();
    foreach($mailbox as $mail) {
      if (!count($ordered_mailbox)) {
        $ordered_mailbox[1] = $mail;
      } else {
        $ordered_mailbox[] = $mail;
      }
    }
    $this->__store_mailbox($ordered_mailbox);
  }
  
  /**
   * Add a mail message to the fake mailbox.
   * 
   * @param array $mail
   */
  public function __deliverMail($mail) {
    $mailbox = $this->__load_mailbox();
    // machinations to ensure the index is 1-based
    if (empty($mailbox)) {
      $index = 1;
    } else {
      $index = max(array_keys($mailbox)) + 1;
    }
    $mail['msgno'] = $index;
    $mail['deleted'] = FALSE;
    $mailbox[$index] = $mail;
    $this->__store_mailbox($mailbox);
  }
  
  //----------------------------------------------------------------------------
  // Redeclarations to make properties public and thus accessible for testing.
  //----------------------------------------------------------------------------

  public $server = NULL;       // a hostname
  public $port = NULL;
  public $protocol = NULL;     // pop3 | imap | pop3s | imaps with the latter two using SSL
  public $ssl = FALSE;         // boolean
  public $login = NULL;
  public $password = NULL;
  public $folder = NULL;       // should usually be 'INBOX'
  
  public function parse_mail_headers_into_array($headers) { 
    return parent::parse_mail_headers_into_array($headers);
  }
  
  public function mail_get_parts($message_number, $part, $prefix) {
    return parent::mail_get_parts($message_number, $part, $prefix);
  }
  
  public function mail_decode_part($message_number, $part, $prefix) { 
    return parent::mail_decode_part($message_number, $part, $prefix);
  }
  
  //----------------------------------------------------------------------------
  // Wrapper methods to generate responses as though from IMAP package functions
  //----------------------------------------------------------------------------
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_body()
   */   
  public function imap_body($message_number, $options = NULL) {
    // munge the message sections together; not terribly realistic, but fine for testing here.
    //
    // this fake behavior is probably very different from what happens in reality when you try
    // imap_body on a multipart message. But since we only try imap_body on non-multipart messages
    // that shouldn't matter all that much.
    $mailbox = $this->__load_mailbox();
    if (isset($mailbox[$message_number])) {
      $body = array();
      foreach ($mailbox[$message_number]['parts'] as $part) {
        if (is_string($part['data'])) {
          $body[] = $part['data'];
        }
      }
      return implode("\n\n", $body);
    }
    return NULL;
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_check()
   */ 
  public function imap_check() {
    $mailbox = $this->__load_mailbox();
    return (object)array(
      'Date' => date(DateTime::RFC2822),
      'Driver' => strtoupper($this->protocol),
      'Mailbox' => $this->folder,
      'Nmsgs' => count($mailbox),
      'Recent' => count($mailbox),
    );
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_close()
   */   
  public function imap_close($flag) {
    if ($this->fail_imap_close) {
      $this->last_error_message = 'imap_close() set to fail';
      return FALSE;
    } else {
      if ($flag == CL_EXPUNGE) {
        $this->__removeMessagesFlaggedForDeletion();
      }
      return TRUE;
    }
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_delete()
   */
  public function imap_delete($message_number) {
    $mailbox = $this->__load_mailbox();
    if (isset($mailbox[$message_number])) {
      $mailbox[$message_number]['deleted'] = TRUE;
    }
    $this->__store_mailbox($mailbox);
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_fetch_overview()
   */ 
  public function imap_fetch_overview($range) {
    $mailbox = $this->__load_mailbox();
    if (preg_match('/:/', $range)) {
      list($start, $end) = explode(':', $range);
      $mails = array();
      foreach($mailbox as $message_number => $mail) {
        if ($message_number >= $start && $message_number <= $end) {
          $mails[$message_number] = $mail;
        }
      }
    } else {
      $message_numbers = explode(',', $range);
      $message_numbers = array_flip($message_numbers);
      $mails = array_intersect_key($mailbox, $message_numbers);
    }
    
    foreach ($mails as &$mail) {
      unset($mail['parts']);
      $mail = (object)$mail;
    }
    return array_values($mails);
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_fetchbody()
   */ 
  public function imap_fetchbody($message_number, $prefix) {
    $mailbox = $this->__load_mailbox();
    if (isset($mailbox[$message_number])) {
      $parts = $mailbox[$message_number]['parts'];
      $headers = array_shift($parts);
      
      if ($prefix === '0') {
        return $headers['data'];
      } else {
        // assuming only one level in the prefix (e.g. it might be 0, 1, 2, etc and not 1.1, 2.3, etc) - which 
        // is not the case in reality, but is fine for these fake mails.
        if (isset($parts[(int)$prefix - 1])) {
          return $parts[(int)$prefix - 1]['data'];
        } else {
          return NULL;
        }
      }
    } else {
      return NULL;
    } 
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_fetchheader()
   */ 
  public function imap_fetchheader($message_number, $options = 0) {
    $mailbox = $this->__load_mailbox();
    if (isset($mailbox[$message_number]['parts'][0]['data'])) {
      return $mailbox[$message_number]['parts'][0]['data'];
    } else {
      return NULL;
    }
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * Of all these wrapper functions, this is probably the one that most butchers and oversimplifies
   * what a real response can look like. 
   * 
   * @see imap_fetchstructure()
   */ 
  public function imap_fetchstructure($message_number, $options = NULL) {
    $mailbox = $this->__load_mailbox();
    if (isset($mailbox[$message_number])) {
      // turning a one-level array (the parts of the mail message data) into a two-level structure of objects. For the purposes of this
      // test harness, we'll treat a one-section-after-header mail as having primary body type = 0 (text) and two-or-more-sections-after-header
      // mails as having primary body type = 1 (multipart)
      //
      // Body type 0 is a single object with no children, not even the remaining body part
      // Body type 1 is an object with a child object for each part following the headers
      $parts = $mailbox[$message_number]['parts'];
      $headers = array_shift($parts);
      $structure = (object)array(
        'type' => 0 // text
      );
      if (count($parts) > 1) {
        $structure->type = 1; // multipart (very fake, broken, lacking-vital-params multipart in this case)
        foreach ($parts as $index => $part) {
          $structure->parts[] = (object)array(
            'type' => 0, // text
            'bytes' => strlen($part['data']),
          ); 
        }
      }
      return $structure;
    } else {
      return NULL;
    } 
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_last_error()
   */
  public function imap_last_error() {
    return $this->last_error_message;
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_mailboxmsginfo()
   */ 
  public function imap_mailboxmsginfo() {
    $mailbox = $this->__load_mailbox();
    $deleted = 0;
    foreach ($mailbox as $mail) {
      if ($mail['deleted']) {
        $deleted++;
      }
    }
    return (object)array (
      'Unread' => count($mailbox),
      'Deleted' => $deleted,
      'Nmsgs' => count($mailbox),
      'Size' => 0, // not important for this use
      'Date' => date(DateTime::RFC2822),
      'Driver' => $this->protocol,
      'Mailbox' => $this->get_mailbox_string(TRUE),
      'Recent' => count($mailbox),
    );
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_open()
   */
  public function imap_open($mailbox, $username, $password, $options = NULL, $n_retries = NULL) {
    if ($this->fail_imap_open) {
      $this->last_error_message = 'imap_open() set to fail';
      return FALSE;
    } else {
      return TRUE;
    }
  }
  
  /**
   * Wrapper method that (incompletely, probably badly) simulates the response of a real server.
   * 
   * @see imap_timeout()
   */
  public function imap_timeout($timeout_type, $timeout) {
    if ($this->fail_imap_open) {
      $this->last_error_message = 'imap_timeout() set to fail';
      return FALSE;
    } else {
      return TRUE;
    }
  } 
  
}