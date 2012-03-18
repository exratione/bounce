<?php

/**
 * @file
 * Hook implementions and other functions relating to outgoing mail.
 */

/**
 * Implements hook_mail_alter().
 *
 * Here we do the following:
 *
 * 1) Set the return path header to the address specified in the admin settings
 * form.
 *
 * 2) Add an identifier to the headers that should help match up non-delivery
 * reports with the email address that the originating mail was sent to.
 *
 * 3) Block outgoing mail to by removing addresses from $message['to'] or
 * setting $message['send'] = FALSE.
 */
function bounce_mail_alter(&$message) {
  $return_path = variable_get('bounce_mail_header_return_path', '');
  if ($return_path) {
    $message['headers']['Return-Path'] = $return_path;
  }

  // Alter the "to" string to remove mails that are blocked. Remember that this
  // is RFC 2822, so could be a comma-delimited string of multiple addresses.
  $to = bounce_mail_remove_blocked_addresses($message['to']);
  if (empty($to)) {
    // No non-blocked addresses to send to, so don't send it.
    $message['send'] = FALSE;
    return;
  }
  else {
    // Update and carry on.
    $message['to'] = $to;
  }

  // Extract emails from the "to" string.
  $emails = _bounce_unique_mails_from_text($message['to']);

  // If there is more than one email then they all get the same header.
  // That makes it more of a pain to figure out which one generated a
  // non-delivery report later on, but oh well.
  $header_id = _bounce_generate_uuid();
  $message['headers'][BOUNCE_MAIL_HEADER_ID_KEY] = $header_id;
  $data = array(
    'header_id' => $header_id,
    'created' => time(),
  );
  foreach ($emails as $email) {
    $data['mail'] = $email;
    drupal_write_record('bounce_sent', $data);
  }
}

/**
 * Given an RFC 2822 set of addresses, remove those that are blocked.
 *
 * @param string $to
 *   The RFC 2822 set of emails to send to.
 *
 * @return string
 *   The amended RFC 2822 addresses string.
 */
function bounce_mail_remove_blocked_addresses($to) {
  $addresses = explode(',', $to);
  $destinations = array();
  foreach ($addresses as $address) {
    $emails = _bounce_unique_mails_from_text($address);
    if ($emails) {
      $destinations[array_shift($emails)] = $address;
    }
  }
  $blocked_mails = db_select('bounce_blocked', 'b')
    ->fields('b', array('mail'))
    ->condition('b.mail', array_keys($destinations), 'IN')
    ->execute()
    ->fetchCol();
  foreach ($blocked_mails as $blocked_mail) {
    unset($destinations[$blocked_mail]);
  }
  return implode(',', $destinations);
}
