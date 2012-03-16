<?php

/**
 * @file
 * Implementation and documentation of hooks defined by the Bounce module.
 */

/**
 * Implements hook_bounce_code_type();
 * 
 * Return the default bounce code types. These are ornamental only, but
 * help keep things usefully categorized.
 */
function bounce_bounce_code_type() {
  $types = array();
  $types['rfc821'] = array(
    'title' => t('RFC 821'),
    'description' => t('SMTP response codes from RFC 821.'),
  );
  $types['rfc1893'] = array(
    'title' => t('RFC 1893'),
    'description' => t('SMTP response codes from RFC 1893.'),
  );
  $types['system'] = array(
    'title' => t('System'),
    'description' => t('Response codes assigned by the Bounce module.'),
  );
  $types['custom'] = array(
    'title' => t('Custom'),
    'description' => t('Custom codes added by admins or other modules.'),
  );
  return $types;
}

/**
 * Implements hook_bounce_code_type_alter().
 * 
 * @param array $types an associative array of code type definitions.
 */
function bounce_bounce_code_type_alter(&$types) {
  // e.g. alter the name of one of the default code types.
  // $types['rfc821']['title'] = t('RFC 821');
}


/**
 * Implements hook_bounce_analyst();
 * 
 * Specify an analysis component that will extract a code and email address from a non-delivery
 * report. The returned array has the form:
 * 
 * array(
 *   'default' => array(
 *     'title' => t('Default non-delivery report analysis'),
 *     'analysis_callback' => 'bounce_analyze_non_delivery_report',
 *     'settings_callback' => 'bounce_analyst_settings',
 *     'configured_check_callback' => 'bounce_analyst_is_configured',
 *     'file' => 'bounce.analysis.inc',   // optional
 *   ),
 *   'another' => array(
 *   	 ...
 *   ),
 *   ...
 * );
 * 
 * CONFIGURED_CHECK_CALLBACK
 * 
 * The configured check callback is called without parameters, and must return TRUE
 * if the analyst component is configured, or FALSE otherwise.
 * 
 * SETTINGS_CALLBACK
 * 
 * The settings_callback must return a form, and has the standard form callback signature:
 * 
 * callback($form, &$form_state)
 * 
 * The form defined will appear as a settings form page in the administration interface 
 * for this module if the analysis component is currently enabled. The settings form 
 * should capture sufficient information for analysis to function.
 * 
 * ANALYSIS CALLBACK
 * 
 * The analysis callback function has the signature:
 * 
 * callback($non_delivery_report)
 * 
 * The paramter $non-delivery_report is an indexed array of message parts. Each message part 
 * is in turn an array containing at least 'charset' and 'data' keys as shown below. The first 
 * part will be mail headers, the rest body and attachment parts. Different mail servers vary 
 * greatly as to how they arrange message parts, so be prepared to look through it all - even 
 * a simple text body may be split up into several parts.
 * 
 * $report = array(
 *  [0] => array(
 *    ['data'] => associative array of headers
 *    ['charset'] => the default encoding of the mail, and thus the headers
 *  ),
 *  [1] => array(
 *    ['data'] => string
 *    ['charset'] => the encoding of the string
 *  ),
 *  [2] => ...
 * )
 * 
 * The callback must return an array containing at least:
 * 
 * array(
 *   'code' => $code or '' // the code assigned to the non-delivery report
 *   'mail' => $mail or '' // the address that the original email was delivered to
 * )
 */
function bounce_bounce_analyst() {
  $analysts = array();
  $analysts['default'] = array(
    'title' => t('Default: simple mail header and body parser'),
    'analysis_callback' => 'bounce_analyze_non_delivery_report',
    'settings_callback' => 'bounce_analyst_settings',
    'configured_check_callback' => 'bounce_analyst_is_configured',
    'file' => 'bounce.analysis.inc',
  );
  return $analysts;
}

/**
 * Implements hook_bounce_analyst_alter().
 * 
 * @param array $analysts an array of analyst component definitions
 * 
 * @see bounce_bounce_analyst()
 */
function bounce_bounce_analyst_alter(&$analysts) {
  // For example, change the name of the default analyst component
  // $analysts['default']['title'] = t('Default: simple mail header and body parser');
}

/**
 * Implements hook_bounce_analysis_alter().
 * 
 * The provided analysis will have the following format:
 * 
 * array(
 *   'code' => $code or '' // the code assigned to the non-delivery report
 *   'mail' => $mail or '' // the address that the original email was delivered to
 * )
 * 
 * @param array $analysis an analysis produced by an analyst component
 * @param array $report a non-delivery report email, formatted as an array of parts
 * 
 * @see bounce_bounce_analyst()
 */
function bounce_bounce_analysis_alter(&$analysis, $report) {
  // For example, change the code assigned based on an examination of the non-delivery report
  // $analysis['code'] = '4.1.1';

  // For preference you should write your own analyst component to assign codes, and declare it
  // through an implementation of hook_bounce_analyst(). But there will always be those times when
  // you want to use an existing analyst component, except for one or two small points of disagreement. 
  // This is a way to implement those minor desired changes.
}

/**
 * Implements hook_bounce_connector().
 * 
 * Specify a connector for communicating with a mail server in order to retrieve
 * non-delivery report emails. The type definitions include callback functions for 
 * implementation and an array of parameters that will be passed to that function.
 * The return from an implementation of this hook takes the form:
 * 
 * array(
 *  'default' => array(
 *    'title' => t('Default POP3/IMAP'),
 *    'reports_callback' => 'bounce_connect',
 *    'settings_callback' => 'bounce_connect_settings',
 *    'configured_check_callback' => 'bounce_connector_is_configured',
 *    'file' => 'mymodule.someinclude.inc',								// optional
 *  ),
 *  'another' = array(
 *    ... 
 *  ),
 *  ...
 * )
 * 
 * CONFIGURED_CHECK_CALLBACK
 * 
 * The configured check callback is called without parameters, and must return TRUE
 * if the connector is configured, or FALSE otherwise.
 * 
 * SETTINGS_CALLBACK
 * 
 * The settings_callback must return a form, and has the standard form callback signature:
 * 
 * callback($form, &$form_state)
 * 
 * The form defined will appear as a settings form page in the administration interface 
 * for this module if the connector is currently enabled. The settings form should capture
 * sufficient information for the connector to function.
 * 
 * REPORTS CALLBACK
 * 
 * The validate_callback function is called without parameters.
 * 
 * A reports_callback function must return an array of non-delivery report emails. Each 
 * non-delivery report in that array is an indexed array of message parts. Each message 
 * part is in turn an array containing at least 'charset' and 'data' keys as shown below. 
 * The first part must be mail headers, the rest body and attachment parts. 
 * 
 * $return_value = array(
 *  [0] => $report,
 *  [1] => $another_report,
 *  [2] => ...
 * )
 * 
 * $report = array(
 *  [0] => array(
 *    ['data'] => associative array of mail headers
 *    ['charset'] => the default encoding of the mail, and thus the headers
 *  ),
 *  [1] => array(
 *    ['data'] => string
 *    ['charset'] => the encoding of the string
 *  ),
 *  [2] => ...
 * )
 * 
 * The message parts must be in the order they occurred in the email. The connector must
 * not return the same non-delivery report more than once; the best practice is to delete
 * non-delivery report emails from the server as soon as they are obtained.
 */
function bounce_bounce_connector() {
  $connectors = array();
  $connectors['default'] = array(
    'title' => t('Default: POP3 or IMAP'),
    'reports_callback' => 'bounce_connect',
    'settings_callback' => 'bounce_connect_settings',
    'configured_check_callback' => 'bounce_connector_is_configured',
    'file' => 'bounce.connector.inc',
  );
  return $connectors;
}

/**
 * Implements hook_bounce_connector_alter().
 * 
 * @param array $connectors an array of connector definitions
 * 
 * @see bounce_bounce_connector()
 */
function bounce_bounce_connector_alter(&$connectors) {
  // For example, change the name of the default connector
  // $connectors['default']['title'] = t('Default: POP3/IMAP');
}

/**
 * Implements hook_bounce_blocker().
 * 
 * Specify a blocker component that will examine stored non-delivery report analysis records
 * and decide which mails must be blocked from receiving future emails. The blocker component
 * also manages deletion of old non-delivery report records.
 * 
 * CONFIGURED_CHECK_CALLBACK
 * 
 * The configured check callback is called without parameters, and must return TRUE
 * if the blocker component is configured, or FALSE otherwise.
 * 
 * SETTINGS_CALLBACK
 * 
 * The settings_callback must return a form, and has the standard form callback signature:
 * 
 * callback($form, &$form_state)
 * 
 * The form defined will appear as a settings form page in the administration interface 
 * for this module if the blocker component is currently enabled. The settings form should capture
 * sufficient information for the blocker to function.
 * 
 * BLOCKED_CALLBACK
 * 
 * The blocked callback is called without parameters, and must return an array of email addresses, 
 * or an empty array. These mails will be added to the list of those currently blocked by the
 * module, and no further outgoing mail will be sent to them.
 * 
 * The blocker should establish which mails to block based on the current contents of the 
 * 'bounce_non_delivery_report' table. Only mails that are not already blocked (i.e. do not
 * already have a record in the 'bounce_blocked' table) should be returned from a call to the
 * block callback.
 * 
 */
function bounce_bounce_blocker() {
  $blockers = array();
  $blockers['default'] = array(
    'title' => t('Default: block by threshold score'),
    'blocked_callback' => 'bounce_blocked',
    'settings_callback' => 'bounce_blocker_settings',
    'configured_check_callback' => 'bounce_blocker_is_configured',
    'file' => 'bounce.blocker.inc',
  );
  return $blockers;
}

/**
 * Implements hook_bounce_blocker_alter().
 * 
 * @param array $blockers an associative array of blocker component definitions
 * 
 * @see bounce_bounce_blocker()
 */
function bounce_bounce_blocker_alter(&$blockers) {
  // For example, change the name of the default blocker component
  // $blockers['default']['title'] = t('Default: block by threshold score');
}

/**
 * Implements hook_bounce_mails_blocked().
 * 
 * @param array $mails an indexed array of email addresses that have just been blocked
 */
function bounce_bounce_mails_blocked($mails) {}

/**
 * Implements hook_bounce_mails_unblocked().
 * 
 * @param array $mails an indexed array of email addresses that have just been unblocked
 */
function bounce_bounce_mails_unblocked($mails) {}