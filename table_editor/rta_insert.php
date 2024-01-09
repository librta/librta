<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
<html>
<!-- ----------------------------------------------------------------- -->
<!--  librta library                                                   -->
<!--  Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)    -->
<!--                                                                   -->
<!--   This program is distributed under the terms of the MIT          -->
<!--   License.  See the file COPYING file.                            -->
<!-- ----------------------------------------------------------------- -->
<head>
<title>Insert Row</title>
</head>
<body>
<?php

    // The user has submitted an insert on a particular
    // table.  Build and execute the INSERT command.
    $tbl  = htmlentities(current($_POST));
    $row  = htmlentities(next($_POST));
    $port= htmlentities(next($_POST));

    // Say where we are.
    print("<center><h3>Insert new row into $tbl</h3></center><hr>\n");

    // Suppress Postgres error messages
    error_reporting(error_reporting() & 0xFFFD);

    // connect to the database 
    $c1 = pg_connect("localhost", "$port", "anyuser");
    if ($c1 == "") { 
        printf("$s%s", "Unable to connect to application.<br>",
            "Please verify that the application is running and ",
            "listening on port $port.<br>");
        exit();
    }

    // Build SQL INSERT command.
    $values = "";
    $command = "INSERT INTO $tbl ( ";
    $count = count($_POST);
    for ($index=3; $index < $count; $index++) {
        // use "htmlentities()" to protect from malicious HTML
        // $value=htmlentities(next($_POST));
        // $key = htmlentities(key($_POST));
        $value=next($_POST);
        $key = key($_POST);
        if ($index > 3) {
            $command = "$command, \"$key\"";
            $values = "$values, \"$value\"";
        }
        else {
            $command = "$command \"$key\"";
            $values = "$values \"$value\"";
        }
    }
    $command = "$command ) VALUES  ( $values )";


    // execute query 
    $r1 = pg_exec($c1, $command);
    if ($r1 == "") { 
        print("<p><font color=\"red\" size=+1>INSERT failed!</p>");
        print("<p>Please verify input values.</font></p>\n");
        print("<p>Command: $command</p>\n");
        exit();
    }

    // INSERT succeeded.  Say so.
    print("<p><font color=\"green\" size=+1>Insert succeeded.");
    print("</font></p>\n<p>Command: $command</p>\n");

    // free the result and close the connection 
    pg_freeresult($r1);
    pg_close($c1);
?>
</body>
</html>
