<?php

/**
 * @file
 * Cron hook implementations and associated functionality.
 */

/**
 * Implements hook_cron_queue_info().
 *
 * Specify queues for connecting to a mail server and for processing of
 * non-delivery report emails.
 */
function bounce_cron_queue_info() {
  $queues = array();
  // a queue to manage connecting to the mail server to retrieve non-delivery reports
  $queues['bounce_connection'] = array(
    'worker callback' => 'bounce_process_connection_queue_item',
    'time' => 60,
  );
  // a queue to process non-delivery reports
  $queues['bounce_non_delivery_report'] = array(
    'worker callback' => 'bounce_process_non_delivery_report_queue_item',
    'time' => 60,
  );
  return $queues;
}

/**
 * Implements hook_cron().
 */
function bounce_cron() {
  // don't run unless the install is complete; all sorts of mischief can happen
  if(variable_get('install_task') != 'done') {
    return;
  }

  // ---------------------------------------------
  // 1) Clear out old data.
  // ---------------------------------------------

  bounce_delete_old_data();

  // ---------------------------------------------
  // 2) Block email addresses where necessary
  // ---------------------------------------------

  // Start by obtaining the blocker
  $blocker = variable_get('bounce_blocker', BOUNCE_DEFAULT_BLOCKER);
  $blockers = bounce_component_definitions('blocker');
  bounce_component_include($blockers[$blocker]);

  // check to see that the blocker is configured
  if ($configured = call_user_func($blockers[$blocker]['configured_check_callback'])) {

    // obtain the list of newly blocked mails
    $mails = call_user_func($blockers[$blocker]['blocked_callback']);

    // drop the offending mails into the blocked table if they aren't there already.
    foreach($mails as $mail) {
      $count = db_select('bounce_blocked')
        ->condition('mail', $mail, '=')
        ->countQuery()
        ->execute()
        ->fetchField();
      if (!$count) {
        // write the record
        $record = array(
          'mail' => $mail,
          'created' => time(),
        );
        drupal_write_record('bounce_blocked', $record);
        // and flag the non-delivery reports for this mail as being used
        db_update('bounce_non_delivery_report')
          ->fields(array(
            'status' => BOUNCE_NDR_STATUS_USED,
          ))
          ->condition('mail', $mail, '=')
          ->execute();
        // and log
        watchdog('bounce', 'Outgoing mail to %mail is now blocked.', array('%mail' => $mail), WATCHDOG_INFO);
      }
    }
    // Call the hook if there were any mails to block.
    if ($mails) {
      module_invoke_all('bounce_mails_blocked', $mails);
    }
  }

  // ----------------------------------------------
  // 3) Add a queue item to connect to the mail server
  // ----------------------------------------------

  $queue = DrupalQueue::get('bounce_connection');
  $queue->createItem(array());
}

/**
 * Clear out the old data that we're no longer using: records of sent mail and non-delivery reports.
 */
function bounce_delete_old_data() {
  // Delete old sent mail records - very straightforward.
  $age_in_seconds = variable_get('bounce_max_sent_age_days', BOUNCE_DEFAULT_MAX_SENT_AGE_DAYS) * 86400;
  $time = time() - $age_in_seconds;
  db_delete('bounce_sent')
    ->condition('created', $time, '<')
    ->execute();

  // Delete old non-delivery reports. This is a little more complex. We want to keep non-delivery reports that were
  // used to block an email address, but only if the email address is still blocked.
  $age_in_seconds = variable_get('bounce_max_report_age_days', BOUNCE_DEFAULT_MAX_REPORT_AGE_DAYS) * 86400;
  $time = time() - $age_in_seconds;
  db_delete('bounce_non_delivery_report')
    ->condition('created', $time, '<')
    ->where('status = :status OR NOT EXISTS (SELECT 1 FROM {bounce_blocked} b WHERE b.mail = {bounce_non_delivery_report}.mail)', array(':status' => BOUNCE_NDR_STATUS_UNUSED))
    ->execute();
}




