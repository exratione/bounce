<?php

/**
 * @file
 * Implementations of user hooks and other integration with the user experience.
 */

/**
 * Implements hook_user_load().
 * 
 * Add a property to the user object to show whether or not the user's email is marked blocked.
 */
function bounce_user_load($users) {
  $query = db_select('bounce_blocked', 'b');
  $query->innerJoin('users', 'u' , 'b.mail = u.mail');
  $blocked_uids = $query->fields('u', array('uid'))
    ->condition('u.uid', array_keys($users), 'IN')
    ->execute()
    ->fetchCol();
  foreach ($users as $uid => $user) {
    if (in_array($uid, $blocked_uids)) {
      $user->bounce_mail_blocked = TRUE;
    } else {
      $user->bounce_mail_blocked = FALSE;
    }
  }
}

/**
 * Implements hook_user_login().
 * 
 * If we are notifying users that their email is blocked, then do so on login.
 */
function bounce_user_login(&$edit, $account) {
  $notify = variable_get('bounce_notify_user_on_login', TRUE);
  if ($notify && $account->bounce_mail_blocked) {
    $message = variable_get('bounce_notify_user_message');
    if($message) {
      drupal_set_message(check_plain($message), 'warning', FALSE);
    }
  }
}

/**
 * Implements hook_form_FORM_ID_alter().
 * 
 * Add a validation method to the password reset form.
 */
function bounce_form_user_register_form_alter(&$form, &$form_state) {
  $form['#validate'][] = 'bounce_user_register_form_validate';
}

/**
 * Validation function for the registration form. Send back an error
 * if a user is registering with a blocked mail, and if the admin settings
 * say we should do that.
 */
function bounce_user_register_form_validate($form, &$form_state) {
  $notify = variable_get('bounce_error_on_registration', TRUE);
  $mail = preg_replace('/^\s+|\s+$/', '', $form_state['values']['mail']);
  if ($notify && $mail) {
    $count = db_select('bounce_blocked', 'b')
      ->condition('b.mail', $mail, '=')
      ->countQuery()
      ->execute()
      ->fetchField();
    $message = variable_get('bounce_error_on_registration_message');
    if($message && $count) {
      form_set_error('mail', check_plain($message));
    }
  }  
}

/**
 * Implements hook_form_FORM_ID_alter().
 * 
 * Add a validation method to the password reset form.
 */
function bounce_form_user_pass_alter(&$form, &$form_state) {
  $form['#validate'][] = 'bounce_user_pass_validate';
}

/**
 * Validation function for the reset password form. Send back an error
 * if a user enters a blocked mail, and if the admin settings
 * say we should do that.
 */
function bounce_user_pass_validate($form, &$form_state) {
  $user = user_load_by_mail($form_state['values']['name']);
  if (!$user) {
    $user = user_load_by_name($form_state['values']['name']);
  }
  if ($user && $user->bounce_mail_blocked) {
    $notify = variable_get('bounce_error_on_password_reset', TRUE);
    $message = variable_get('bounce_error_on_password_reset_message');
    if($message && $notify) {
      form_set_error('mail', check_plain($message));
    }
  }
}
