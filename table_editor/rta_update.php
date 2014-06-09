<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
<html>
<head>
<title>Edit Row</title>
</head>
<body>
<?php
    // The user has submitted an update to a particular
    // row in a particular table.  Build and execute 
    // the UPDATE command.
    $tbl = htmlentities(current($_POST));
    $row = htmlentities(next($_POST));

    // Say where we are.
    print("<center><h3>Update $tbl, row $row</h3></center><hr>\n");

    // Suppress Postgres error messages
    error_reporting(error_reporting() & 0xFFFD);

    // connect to the database 
    $c1 = pg_connect("localhost", "8888", "anyuser");
    if ($c1 == "") { 
        printf("$s%s", "Unable to connect to application.<br>",
            "Please verify that the application is running and ",
            "listening on port 8888.<br>");
        exit();
    }

    // Build SQL UPDATE command.
    $command = "UPDATE $tbl SET ";
    $count = count($_POST);
    for ($index=2; $index < $count; $index++) {
        // use "htmlentities()" to protect from malicious HTML
        // $value=htmlentities(next($_POST));
        // $key = htmlentities(key($_POST));
        $value=next($_POST);
        $key = key($_POST);
        if ($index > 2)
            $command = "$command, '$key' = '$value' ";
        else
            $command = "$command '$key' = '$value' ";
    }
    $command = "$command LIMIT 1 OFFSET $row";


    // execute query 
    $r1 = pg_exec($c1, $command);
    if ($r1 == "") { 
        print("<p><font color=\"red\" size=+1>Update failed!</p>");
        print("<p>Please verify input values.</font></p>\n");
        print("<p>Command: $command</p>\n");
        exit();
    }

    // Update succeeded.  Say so.
    print("<p><font color=\"green\" size=+1>Update succeeded.");
    print("</font></p>\n<p>Command: $command</p>\n");

    // free the result and close the connection 
    pg_freeresult($r1);
    pg_close($c1);
?>
</body>
</html>
