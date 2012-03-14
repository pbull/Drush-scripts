<?php

/**
 * @file
 * Additional Drush integration for the redirect module.
 */

/**
 * Implementation of hook_drush_command().
 */
function redirect_import_drush_command() {
  $items['import-redirects'] = array(
    'description' => 'Import redirects from CSV or .htaccess file.',
    'drupal dependencies' => array('redirect'),
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
function drush_redirect_import_import_redirects($file = NULL) {
  $allowed_file_types = array('csv', 'htaccess');
  
  if (!$file) {
    drush_set_error('IMPORT_REDIRECTS', dt('File must be specified as an argument.'));
    return;
  }
  elseif (!is_file($file)) {
    drush_set_error('IMPORT_REDIRECTS', dt('@path is not a valid file path.', array('@path' => $file)));
    return;
  }
  
  // determine file type
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
        drush_set_error('IMPORT_REDIRECTS',
          dt('File type could not be identified. Please specify --type=(@types) when running this command.',
          array('@types' => implode(',', $allowed_file_types))));
        return;
      }
    }
  }
  
  switch ($type) {
    case 'csv':
      drush_print(t('Importing redirects from CSV file @file', array('@file' => $file)));
      drush_redirect_import_csv($file);
      break;
    case 'htaccess':
      drush_print(t('Importing .htaccess redirects from @file', array('@file' => $file)));
      drush_redirect_import_htaccess($file);
      break;
  }
  
}

function drush_redirect_import_csv($filepath) {
  $count_add = 0;
  $count_skip = 0;
  
  // Loop through csv file
  if (($handle = fopen($filepath, "r")) !== FALSE) {
    
    // Delete existing redirects
    drush_redirect_delete_all_redirects();
    
    $status_code_options = redirect_status_code_options();
    while (($data = fgetcsv($handle, 1000, ",")) !== FALSE) {     //1000????
      
      // status code validation
      $status_code = (count($data) > 2) ? trim($data[2]) : NULL;
      if (is_null($status_code) || !array_key_exists($status_code, $status_code_options)) {
        $status_code = variable_get('redirect_default_status_code', 301);
      }
      
      // clean up source
      $source = str_replace(url('', array('absolute' => TRUE)), '', trim($data[0]));
      
      // clean up target if it's not a full url
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
      
      // add redirect
      if ($source && $target && ($source != $target)) {
        $redirect = new stdClass();
        redirect_object_prepare($redirect);
        $redirect->source = $source;
        $redirect->redirect = $target;
        $redirect->status_code = $status_code;
        
        // Check whether source path already exists
        redirect_hash($redirect);
        if ($existing = redirect_load_by_hash($redirect->hash)) {
          
          // TODO: we could update existing redirects if we add a cli option
          
          $count_skip++;
          drush_print(t('Redirect from @source already exists. Skipping this entry.', array('@source' => $source)));
        }
        else {
          redirect_save($redirect);
          $count_add++;
          
          if (drush_get_context('DRUSH_VERBOSE')) {
            drush_print(t('Redirecting @source to @target (with status code @status_code)',
              array('@source' => $source, '@target' => $target, '@status_code' => $status_code)));
          }
          
        }
        
        // Dump redirect object
        if (drush_get_context('DRUSH_DEBUG')) {
          drush_print_r($redirect);
        }
        
      }
      
    }
    fclose($handle);
    drush_print(t('Imported @count redirects.', array('@count' => $count_add)));
    drush_print(t('Skipped @count duplicate or previously existing redirects.', array('@count' => $count_skip)));
    return;
  }
  else {
    drush_set_error('IMPORT_REDIRECTS', dt('Unable to open filefor reading.'));
    return;
  }
}

function drush_redirect_import_htaccess($filepath) {
  
}

/**
 * Delete all existing redirects from the database.
 */
function drush_redirect_delete_all_redirects() {
  $delete = drush_get_option('delete');
  if ($delete) {
    $rids = db_query('SELECT rid FROM {redirect}', array())->fetchCol();
    redirect_delete_multiple($rids);
    drush_print(t('Deleted @count existing redirects.', array('@count' => count($rids))));
  }
}
