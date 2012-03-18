<?php

/**
 * @file
 * Definition of BounceTestMailServerConnector.
 */

/**
 * A class for connecting to a mail server account via POP3 or IMAP protocols.
 *
 * The class needs the php-imap package installed: http://www.php.net/manual/en/book.imap.php
 * On a Fedora OS, "yum install php-imap" will do the trick.
 * Yes, the php-imap package manages POP as well as IMAP.
 *
 * The innards of this class are adapted from code in a comment by Wil Barath: http://www.php.net/manual/en/book.imap.php#96414
 * It uses much of that material, while adding comments and fixing a number of issues discovered in the course of testing.
 */
class BounceMailServerConnector {

  protected $connection = NULL;

  protected $server = NULL;       // a hostname
  protected $port = NULL;
  protected $protocol = NULL;     // pop3 | imap | pop3s | imaps with the latter two using SSL
  protected $ssl = FALSE;         // boolean
  protected $login = NULL;
  protected $password = NULL;
  protected $folder = NULL;       // should usually be 'INBOX'

  /**
   * Don't retrieve a message part larger than this value, as it's unlikely to be
   * text, and we're only interested in text.
   */
  protected $max_mail_part_size_bytes = NULL;

  /**
   * The class constructor. The provided parameter array must be of the form:
   *
   * array(
   *   'server' => 'mail.mydomain.com',
   *   'protocol' => 'pop3s',
   *   'port' => 993,
   *   'login' => 'me@mydomain.com',
   *   'password' => 'some string',
   *   'folder' => 'INBOX',
   *   'timeout' => 5,
   * )
   *
   * The available values for the protocol parameter are: ['pop3', 'pop3s', 'imap', 'imaps']. The 's' versions run over SSL.
   *
   * The folder parameter should almost always be 'INBOX'.
   *
   * @param array $params
   *   The parameters for connecting to a mail server.
   */
  public function __construct($params) {
    $this->server = $params['server'];
    $this->port = $params['port'];
    $this->login = $params['login'];
    $this->password = $params['password'];
    $this->folder = $params['folder'];
    $this->max_mail_part_size_bytes = $params['mail_part_size_limit'];

    switch ($params['protocol']) {
      case 'imap':
        $this->protocol = 'imap';
        break;
      case 'imaps':
        $this->protocol = 'imap';
        $this->ssl = TRUE;
      case 'pop3':
        $this->protocol = 'pop3';
        break;
      case 'pop3s':
      default:
        $this->protocol = 'pop3';
        $this->ssl = TRUE;
        break;
    }

    // set the connection timeouts
    $result = $this->imap_timeout(IMAP_OPENTIMEOUT, $params['timeout']);
    if (!$result) {
      $this->watchdog_last_imap_error('Failed to set open timeout');
    }
    $result = $this->imap_timeout(IMAP_READTIMEOUT, $params['timeout']);
    if (!$result) {
      $this->watchdog_last_imap_error('Failed to set read timeout');
    }
    $result = $this->imap_timeout(IMAP_WRITETIMEOUT, $params['timeout']);
    if (!$result) {
      $this->watchdog_last_imap_error('Failed to set write timeout');
    }
    $result = $this->imap_timeout(IMAP_CLOSETIMEOUT, $params['timeout']);
    if (!$result) {
      $this->watchdog_last_imap_error('Failed to set close timeout');
    }
  }

  /**
   * Connect and log in to a mail server.
   *
   * @return bool TRUE on success, FALSE on error.
   */
  public function login() {
    // The mail server may be self-signed if it is using SSL, so "novalidate-cert" is generally necessary. It might even be needed
    // for non-SSL connections, depending on how the server is configured - and it doesn't hurt to have it there regardless.
    $sslpart = ($this->ssl) ? 'ssl/novalidate-cert' : 'novalidate-cert';
    try {
      $this->connection = $this->imap_open(
        $this->get_mailbox_string(),
        $this->login,
        $this->password,
        OP_SILENT // Why this? See https://bugs.php.net/bug.php?id=33039 and http://www.php.net/manual/en/function.imap-open.php#73514
      );
      if (!$this->connection) {
        $this->watchdog_last_imap_error(t('Login failure'));
        return FALSE;
      } else {
        return TRUE;
      }
    } catch (Exception $e) {
      // plausible to see exceptions if very bad values have been passed in, such as an empty hostname string
      $this->watchdog_last_imap_error($e->getMessage());
      return FALSE;
    }
  }

  /**
   * Log out of the currently active POP3 server connection.
   */
  public function logout() {
    if ($this->connection) {
      // close connection and delete all messages flagged for deletion
      $result = $this->imap_close(CL_EXPUNGE);
      $this->connection = NULL;
      if (!$result) {
        $this->watchdog_last_imap_error(t('Logout failure'));
      }
    }
  }

  /**
   * Obtain information on the POP3 folder you are presently logged in to.
   * The function returns a result of this form:
   *
   * Array (
   *   [Unread] => 0
   *   [Deleted] => 0
   *   [Nmsgs] => 0
   *   [Size] => 0
   *   [Date] => Mon, 7 Feb 2011 22:28:23 -0500 (EST)
   *   [Driver] => pop3
   *   [Mailbox] => {m.host.com:995/pop3/ssl/user="us@host.com"}INBOX
   *   [Recent] => 0
   * )
   *
   * Where Nmsgs is the total number of messages in the folder, and the other
   * parameters should be fairly self-explanatory.
   *
   * @return array
   */
  public function get_mailbox_info() {
    $check = $this->imap_mailboxmsginfo();
    if ($check) {
      return ((array)$check);
    } else {
      $this->watchdog_last_imap_error(t('Failed to get mailbox info'));
    }
  }

  /**
   * List messages in the presently active POP3 mail folder.
   * The function produces results of this form: a list of message information:
   *
   * Array (
   *   [1] => Array (
   *     [subject] => test w/ attachment
   *     [from] => name
   *     [to] => mail@domain.com
   *     [date] => Mon, 7 Feb 2011 19:37:07 -0800
   *     [message_id] =>
   *     [size] => 962
   *     [uid] => 1
   *     [msgno] => 1
   *     [recent] => 1
   *     [flagged] => 0
   *     [answered] => 0
   *     [deleted] => 0
   *     [seen] => 0
   *     [draft] => 0
   *     [udate] => 1297136227
   *   )
   *   [2] => ...
   * )
   *
   * @param string $range
   *   In the form x:y e.g. 1:3 obtains messages 1, 2, and 3.
   * @return array
   *   The messages.
   */
  public function list_messages($range = '') {
    $result = array();
    if (!$range) {
      $mc = $this->imap_check();
      if (!$mc) {
        $this->watchdog_last_imap_error(t('Failed to check the mailbox for the number of messages'));
        return $result;
      }
      $range = '1:' . $mc->Nmsgs;
    }
    $response = $this->imap_fetch_overview($range);
    if (!is_array($response)) {
      $this->watchdog_last_imap_error(t('Failed to obtain an overview of messages'));
      return $result;
    }
    foreach ($response as $message) {
      $result[$message->msgno] = (array)$message;
    }
    return $result;
  }

  /**
   * Return the headers of a message, either as raw text or an array indexed by header name.
   *
   * @param integer $message_number
   *   An integer index key from the array returned by list_messages().
   * @param bool $headers_as_array
   *   If true, return an array rather than the raw header string.
   * @return mixed
   *   Either a header string or the headers formed up as an array.
   */
  public function get_message_headers($message_number, $headers_as_array = false) {
    $headers = $this->imap_fetchheader($message_number, FT_PREFETCHTEXT);
    if (!is_string($headers)) {
      $headers = '';
      $this->watchdog_last_imap_error(t('Failed to retrieve the header for message number = %id', array('%id' => $message_number)));
    }
    if ($headers_as_array) {
      $headers = $this->parse_mail_headers_into_array($headers);
    }
    return $headers;
  }

  /**
   * Mark the the indicated message for deletion. It will be deleted when logout() is called.
   *
   * @param integer $message_number
   *   An integer index key from the array returned by list_messages().
   * @return bool
   *   TRUE on success, FALSE otherwise.
   */
  public function mark_message_for_deletion($message_number) {
    $result = $this->imap_delete($message_number);
    if (!$result) {
      $this->watchdog_last_imap_error(t('Failed to delete message number = %id', array('%id' => $message_number)));
    }
    return $result;
  }


  /**
   * Return an array containing data from the various message parts.
   * The results look much like this for a single part email, with the
   * message headers in raw and parsed form contained in the first array field.
   *
   * Array (
   *   [0] => Array (
   *     [charset] => us-ascii
   *     [raw] => Return-Path:
   *               Delivered-To: account@domain.com
   *               Reply-To:
   *               From: "That Guy"
   *               To:
   *               Subject: test 1
   *               Date: Mon, 7 Feb 2011 19:37:07 -0800
   *               Message-ID:
   *               MIME-Version: 1.0
   *               Content-Type: text/plain;
   *               charset="us-ascii"
   *               Content-Transfer-Encoding: 7bit
   *               Content-Language: en-us
   *     [data] => Array(
   *       [Return-Path] =>
   *       [Delivered-To] => account@domain.com
   *       [Reply-To] =>
   *       [From] => "That Guy"
   *       [To] =>
   *       [Subject] => test 1
   *       [Date] => Mon, 7 Feb 2011 19:37:07 -0800
   *       [Message-ID] =>
   *       [MIME-Version] => 1.0
   *       [Content-Type] => text/plain;charset="us-ascii"
   *       [Content-Transfer-Encoding] => 7bit
   *     )
   *   )
   *   [1] => Array (
   *     [charset] => us-ascii
   *     [data] => example mail body, probably much longer in reality
   *   )
   * )
   *
   * Multi-part mails will contain more array entries of data, one for each part
   * or file attachment.
   *
   * @param integer $message_number
   *   The message identifier.
   * @return array
   *   The email.
   */
  public function get_message($message_number) {
    $mail = $this->imap_fetchstructure($message_number);
    if (!is_object($mail)) {
      $this->watchdog_last_imap_error(t('Failed to retrieve message structure for message number = %id', array('%id' => $message_number)));
      return array();
    }

    // $this->mail_get_parts() will chase down all the components of a multi-part mail,
    // but only return the headers of a single part mail
    $mail = $this->mail_get_parts($message_number, $mail, '0');
    $mail[0]['raw'] = $mail[0]['data'];
    $mail[0]['data'] = $this->parse_mail_headers_into_array($mail[0]['raw']);
    if(!isset($mail[0]['charset'])) {
      $mail[0]['charset'] = 'utf-8'; // probably wrong, but better than nothing
    };

    // so if it is only a single part mail, we have to go and fetch the body
    if (count($mail) == 1) {
      $body = $this->imap_body($message_number);
      if (!is_string($body)) {
        $this->watchdog_last_imap_error(t('Failed to retrieve message body for message number = %id', array('%id' => $message_number)));
      }
      $mail[] = array(
        'data' => $body,
        'charset' => $mail[0]['charset'],
      );
    }

    return $mail;
  }

  //---------------------------------------------------
  // Utility functions from here on down
  //---------------------------------------------------

  /**
   * Return the connection string for the mailbox, of the form:
   *
   * {mail.host.com:995/pop3/ssl/novalidate-cert}INBOX
   * {mail.host.com:995/pop3/ssl/novalidate-cert/user="us@host.com"}INBOX
   *
   * @param boolean $with_user
   *   If TRUE, include the user="userlogin" segment
   * @return string
   *   The connection string.
   */
  protected function get_mailbox_string($with_user = FALSE) {
    $inner = $this->server . ':' . $this->port . '/' . $this->protocol . '/';
    $inner .= ($this->ssl) ? 'ssl/novalidate-cert' : 'novalidate-cert';
    if ($with_user) {
      $inner .= '/' . '"user=' . $this->login . '"';
    }
    return '{' . $inner . '}' . $this->folder;
  }

  /**
   * Record the last error generated from the underlying IMAP functionality.
   *
   * @param string $message
   */
  protected function watchdog_last_imap_error($message) {
    $error = $this->imap_last_error();
    if ($error) {
      watchdog('bounce', '%message : %error', array('%message' => $message, '%error' => $error), WATCHDOG_ERROR);
    } else {
      watchdog('bounce', '%message : No IMAP package error found.', array('%message' => $message), WATCHDOG_ERROR);
    }
  }

  /**
   * Parse a message header into an associative array of name-value pairs.
   *
   * @param string $headers
   *   The email headers as a string.
   * @return array
   *   An associated array of name-value pairs.
   */
  protected function parse_mail_headers_into_array($headers) {
     $headers = preg_replace('/\r\n\s+/m', '', $headers);
     preg_match_all('/([^: ]+): (.+?(?:\r\n\s(?:.+?))*)?\r\n/m', $headers, $matches);
     $result = array();
     foreach( $matches[1] as $key => $value ) {
        $result[$value] = $matches[2][$key];
     }
     return $result;
  }

  /**
   * Recursively walk through the parts of a multi-part mail, obtaining the content for each part.
   *
   * @param integer $message_number
   *   The identifier for the message.
   * @param object $part
   *   A message part.
   * @param string $prefix
   * @return array
   *   The parts of the mail as an array.
   */
  protected function mail_get_parts($message_number, $part, $prefix) {
    $attachments = array();
    $attachments[$prefix] = $this->mail_decode_part($message_number, $part, $prefix);
    if (isset($part->parts)) {
      // this is multipart
      $prefix = ($prefix == '0') ? '' : $prefix . '.';
      foreach ($part->parts as $index => $subpart) {
        $attachments = array_merge(
          $attachments,
          // $index below should be 0-based, but what needs to be passed into the server is 1-based, hence the + 1
          $this->mail_get_parts($message_number, $subpart, $prefix . ($index + 1))
        );
      }
    }
    return $attachments;
  }

  /**
   * Retrieve and decode the content for a single part, or return a message
   * if the part is larger than the limit.
   *
   * @param integer $message_number
   *   The identifier for the message,
   * @param object $part
   *   A message part.
   * @param string $prefix
   * @return array
   *   The part data.
   */
  protected function mail_decode_part($message_number, $part, $prefix) {
    $attachment = array();

    if (isset($part->ifdparameters) && $part->ifdparameters) {
      foreach ($part->dparameters as $object) {
        $attachment[drupal_strtolower($object->attribute)] = $object->value;
        if (drupal_strtolower($object->attribute) == 'filename') {
          $attachment['is_attachment'] = true;
          $attachment['filename'] = $object->value;
        }
      }
    }

    if (isset($part->ifparameters) && $part->ifparameters ) {
      foreach ($part->parameters as $object ) {
        $attachment[drupal_strtolower($object->attribute)] = $object->value;
        if (drupal_strtolower($object->attribute) == 'name' ) {
          $attachment['is_attachment'] = true;
          $attachment['name'] = $object->value;
        }
      }
    }

    // If this thing is large, just return a string saying it is large. Large items are generally not what
    // we are looking for when searching for bounce-related information.
    //
    // Note that if it has sub-parts, the byte count should be a sum of subpart byte counts, so ignore it.
    if (!isset($part->parts) && isset($part->bytes) && $part->bytes > $this->max_mail_part_size_bytes) {
      $attachment['data'] = t('Mail part too large to consider: @bytes bytes', array('@bytes' => $part->bytes));
      return $attachment;
    }

    $data = $this->imap_fetchbody($message_number, $prefix);
    if (!is_string($data)) {
      $this->watchdog_last_imap_error(t('Failed to retrieve message structure for message number = %id', array('%id' => $message_number)));
      $attachment['data'] = '';
    } else {
      if (isset($part->encoding)) {
        if ($part->encoding == 3) { // 3 = BASE64
          $attachment['data'] = base64_decode($attachment['data']);
        } elseif ($part->encoding == 4) { // 4 = QUOTED-PRINTABLE
          $attachment['data'] = quoted_printable_decode($attachment['data']);
        }
      }
      $attachment['data'] = $data;
    }
    return $attachment;
  }

  //---------------------------------------------------
  // Wrapper methods for IMAP functions.
  //---------------------------------------------------

  // These functions are wrapped to allow overriding in child classes, e.g. for testing without the need for a mailserver.

  /**
   * Wrapper method.
   *
   * @see imap_body()
   */
  protected function imap_body($message_number, $options = NULL) {
    return imap_body($this->connection, $message_number, $options);
  }

  /**
   * Wrapper method.
   *
   * @see imap_check()
   */
  protected function imap_check() {
    return imap_check($this->connection);
  }

  /**
   * Wrapper method.
   *
   * @see imap_close()
   */
  protected function imap_close($flag) {
    return imap_close($this->connection, $flag);
  }

  /**
   * Wrapper method.
   *
   * @see imap_delete()
   */
  protected function imap_delete($message_number) {
    return imap_delete($this->connection, $message_number);
  }

  /**
   * Wrapper method.
   *
   * @see imap_fetch_overview()
   */
  protected function imap_fetch_overview($range) {
    return imap_fetch_overview($this->connection, $range);
  }

  /**
   * Wrapper method.
   *
   * @see imap_fetchbody()
   */
  protected function imap_fetchbody($message_number, $prefix) {
    return imap_fetchbody($this->connection, $message_number, $prefix);
  }

  /**
   * Wrapper method.
   *
   * @see imap_fetchheader()
   */
  protected function imap_fetchheader($message_number, $options = 0) {
    return imap_fetchheader($this->connection, $message_number, $options);
  }

  /**
   * Wrapper method.
   *
   * @see imap_fetchstructure()
   */
  protected function imap_fetchstructure($message_number, $options = NULL) {
    return imap_fetchstructure($this->connection, $message_number, $options);
  }

  /**
   * Wrapper method.
   *
   * @see imap_last_error()
   */
  protected function imap_last_error() {
    return imap_last_error();
  }

  /**
   * Wrapper method.
   *
   * @see imap_mailboxmsginfo()
   */
  protected function imap_mailboxmsginfo() {
    return imap_mailboxmsginfo($this->connection);
  }

  /**
   * Wrapper method.
   *
   * @see imap_open()
   */
  protected function imap_open($mailbox, $username, $password, $options = NULL, $n_retries = NULL) {
    return imap_open($mailbox, $username, $password, $options, $n_retries);
  }

  /**
   * Wrapper method.
   *
   * @see imap_timeout()
   */
  protected function imap_timeout($timeout_type, $timeout) {
    return imap_timeout($timeout_type, $timeout);
  }
}