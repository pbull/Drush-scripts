<?php

/**
 * @file
 * Additional Drush integration for the path_redirect module.
 */


/**
 * Implementation of hook_drush_command().
 */
function path_redirect_import_drush_command() {
  $items['import-path-redirects'] = array(
    'description' => 'Import redirects from CSV or .htaccess file, using Path Redirect module.',
    'drupal dependencies' => array('path_redirect'),
    'arguments' => array(
      'file' => 'Path of file to import.',
    ),
    'options' => array(
      'delete' => 'Delete all redirects before initializing import.',
      'type' => 'Force evaluation of file type (csv, htaccess)',
    ),
  );
  
  return $items;
}

/**
 * Command callback. Generate a number of redirects.
 */
function drush_path_redirect_import_import_path_redirects($file = NULL) {
  $allowed_file_types = array('csv', 'htaccess');
  
  if (!$file) {
    drush_set_error('IMPORT_PATH_REDIRECTS', dt('File must be specified as an argument.'));
    return;
  }
  elseif (!is_file($file)) {
    drush_set_error('IMPORT_PATH_REDIRECTS', dt('@path is not a valid file path.', array('@path' => $file)));
    return;
  }
  
  // Include path_redirect.admin.inc
  drush_path_redirect_import_include_module();
  
  // Determine file type
  $type = str_replace('.', '', strtolower(drush_get_option('type')));
  if (!in_array($type, $allowed_file_types)) {
    // check extension
    $pathinfo = pathinfo($file);
    $type = strtolower($pathinfo['extension']);
    if (!in_array($type, $allowed_file_types)) {
      // check file name
      if (strpos(strtolower($pathinfo['filename']), 'htaccess') !== FALSE) {
        $type = 'htaccess';
      }
      // no file type identifed
      else {
        drush_set_error('IMPORT_PATH_REDIRECTS',
          dt('File type could not be identified. Please specify --type=(@types) when running this command.',
          array('@types' => implode(',', $allowed_file_types))));
        return;
      }
    }
  }
  
  // Delete existing redirects
  if (drush_get_option('delete')) {
    drush_path_redirect_import_delete_all_redirects();
  }
  
  switch ($type) {
    case 'csv':
      drush_print(dt('Importing redirects from CSV file @file', array('@file' => $file)));
      drush_path_redirect_import_csv($file);
      break;
    case 'htaccess':
      drush_print(dt('Importing .htaccess redirects from @file', array('@file' => $file)));
      drush_path_redirect_import_htaccess($file);
      break;
  }
  
}

/**
 * Parse CSV file and import redirects.
 */
function drush_path_redirect_import_csv($filepath) {
  $count_add = 0;
  $count_skip = 0;
  
  // Loop through csv file
  if (($handle = fopen($filepath, "r")) !== FALSE) {
    
    $status_code_options = path_redirect_status_code_options();
    while (($data = fgetcsv($handle, 1000, ",")) !== FALSE) {     //1000????
      
      // Status code validation
      $status_code = (count($data) > 2) ? trim($data[2]) : NULL;
      if (is_null($status_code) || !array_key_exists($status_code, $status_code_options)) {
        $status_code = variable_get('path_redirect_default_status', 301);
      }
      
      // Clean up source
      $source = str_replace(url('', array('absolute' => TRUE)), '', trim($data[0]));
      
      // Clean up target if it's not a full url
      $target = trim($data[1]);
      if (!preg_match("/^https?:\/\//", $target)) {
        $arr_t = explode("/", $target);
        foreach ($arr_t as $k => $v) {
          if (!trim($v)) {
            unset($arr_t[$k]);
          }
        }
        $target = implode("/", $arr_t);
      }
      
      drush_path_redirect_import_save_redirect($source, $target, $status_code, $count_add, $count_skip);
      
    }
    fclose($handle);
    drush_print(t('Imported @count redirects.', array('@count' => $count_add)));
    drush_print(t('Skipped @count duplicate or previously existing redirects.', array('@count' => $count_skip)));
    return;
  }
  else {
    drush_set_error('IMPORT_PATH_REDIRECTS', dt('Unable to open file for reading.'));
    return;
  }
}

/**
 * Parse Redirect entries in .htaccess file and import redirects.
 */
function drush_path_redirect_import_htaccess($filepath) {
  $count_add = 0;
  $count_skip = 0;
  
  // Loop through csv file
  if (($handle = fopen($filepath, "r")) !== FALSE) {
    $status_code_options = path_redirect_status_code_options();
    $data = fread($handle, filesize($filepath));
    
    // Match Redirect directives
    preg_match_all('/redirect\s+(\d+)\s+(.*?)\s+(.*?)\s/i', $data, $matches, PREG_SET_ORDER);
    foreach ($matches as $match) {
      
      // Status code validation
      $status_code = trim($match[1]);
      if (!array_key_exists($status_code, $status_code_options)) {
        $status_code = variable_get('path_redirect_default_status', 301);
      }
      
      // Clean up source
      $source = str_replace(url('', array('absolute' => TRUE)), '', $match[2]);
      $source = preg_replace('/^\//', '', $source);
      
      // Clean up target
      $target = str_replace(url('', array('absolute' => TRUE)), '', $match[3]);
      if (!preg_match("/^https?:\/\//", $target)) {
        $target = preg_replace('/^\//', '', $target);
      }
      
      drush_path_redirect_import_save_redirect($source, $target, $status_code, $count_add, $count_skip);
      
    }
    fclose($handle);
    drush_print(t('Imported @count redirects.', array('@count' => $count_add)));
    drush_print(t('Skipped @count duplicate or previously existing redirects.', array('@count' => $count_skip)));
    return;
  }
  else {
    drush_set_error('IMPORT_PATH_REDIRECTS', dt('Unable to open file for reading.'));
    return;
  }
}

/**
 * Delete all existing redirects from the database.
 */
function drush_path_redirect_import_delete_all_redirects() {
  $query = db_query("SELECT rid FROM {path_redirect}");
  $rids = array();
  while ($rid = db_result($query)) {
    $rids[] = $rid;
  }
  
  if ($rids) {
    path_redirect_delete_multiple($rids);
    drush_print(dt('Deleted @count existing redirects from database.', array('@count' => count($rids))));
    if (drush_get_context('DRUSH_VERBOSE')) {
      drush_print_r($rids);
    }
  }
  else {
    drush_print(dt('No existing redirects to be deleted.'));
  }
}

/**
 * Add new redirect.
 */
function drush_path_redirect_import_save_redirect($source, $target, $status_code, &$count_add, &$count_skip) {
  //if ($source && $target && ($source != $target)) {
  if ($source && $target) { // && ($source != $target)) {
    $redirect = array(
      'source' => $source,
      'redirect' => $target,
      'type' => $status_code,
      'language' => '',
    );
    
    // Dump redirect object
    if (drush_get_context('DRUSH_DEBUG')) {
      drush_print_r($redirect);
    }
    
    // Validation based on path_redirect_edit_form_validate()
    // Check whether source path already exists
    if ($existing = path_redirect_load_by_source($redirect['source'], $redirect['language'])) {
      //if ($redirect['rid'] != $existing['rid'] && $redirect['language'] == $existing['language']) {
        // The "from" path should not conflict with another redirect
        drush_set_error('IMPORT_PATH_REDIRECTS', dt('Redirect from @source already exists. Skipping this entry.', array('@source' => $source)));
        $count_skip++;
      //}
    }
    
    // Check that there there are no redirect loops
    elseif (url(drupal_strtolower($redirect['source'])) == url(drupal_strtolower($redirect['redirect']))) {
      drush_set_error('IMPORT_PATH_REDIRECTS',
        dt('You are attempting to redirect the path @source to itself. This will result in an infinite loop.', array('@source' => $source)));
      $count_skip++;
    }
    
    // Save redirect
    else {
      path_redirect_save($redirect);
      $count_add++;
      
      if (drush_get_context('DRUSH_VERBOSE')) {
        drush_print(dt('Redirecting @source to @target (with status code @status_code)',
          array('@source' => $source, '@target' => $target, '@status_code' => $status_code)));
      }
    }
    
  }
}

/**
 * Include path_redirect.admin.inc
 */
function drush_path_redirect_import_include_module() {
  $path = drupal_get_path('module', 'path_redirect');
  require_once($path . '/path_redirect.admin.inc');
}
