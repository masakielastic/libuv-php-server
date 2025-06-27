<?php
// Test script for Aurora extension

// Load the extension
if (!extension_loaded('aurora')) {
    if (!dl('./modules/aurora.so')) {
        die("Failed to load Aurora extension\n");
    }
}

echo "Aurora extension loaded successfully!\n";

// Check if functions are available
$functions = ['aurora_serve', 'aurora_register_handler', 'aurora_stop'];
foreach ($functions as $func) {
    if (function_exists($func)) {
        echo "✓ Function $func is available\n";
    } else {
        echo "✗ Function $func is NOT available\n";
    }
}

// Display extension info
if (function_exists('phpinfo')) {
    echo "\n--- Aurora Extension Info ---\n";
    ob_start();
    phpinfo(INFO_MODULES);
    $info = ob_get_contents();
    ob_end_clean();
    
    if (strpos($info, 'aurora') !== false) {
        echo "Aurora module info found in phpinfo()\n";
    } else {
        echo "Aurora module info NOT found in phpinfo()\n";
    }
}

echo "\nTest completed!\n";
?>