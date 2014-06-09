<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
<html>
<!-- ----------------------------------------------------------------- -->
<!--  Run Time Access                                                  -->
<!--  Copyright (C) 2003-2008 Robert W Smith (bsmith@linuxtoys.org)    -->
<!--                                                                   -->
<!--   This program is distributed under the terms of the GNU          -->
<!--   LGPL.  See the file COPYING file.                               -->
<!-- ----------------------------------------------------------------- -->
<head>
<title>Delete Row</title>
</head>
<body>
<?php

    // The user has asked to delet a particular
    // row in a particular table.  Build and execute 
    // the DELETE command.
    $tbl  = htmlentities($_GET[table]);
    $row  = htmlentities($_GET[row]);
    $port = htmlentities($_GET[port]);

    // Say where we are.
    print("<center><h3>Delete $tbl, row $row</h3></center><hr>\n");

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

    // Build SQL DELETE command.
    $command = "DELETE FROM $tbl LIMIT 1 OFFSET $row";

    // execute query 
    $r1 = pg_exec($c1, $command);
    if ($r1 == "") { 
        print("<p><font color=\"red\" size=+1>Update failed!</p>");
        print("<p>Please verify input values.</font></p>\n");
        print("<p>Command: $command</p>\n");
        exit();
    }

    // Update succeeded.  Say so.
    print("<p><font color=\"green\" size=+1>Delete succeeded.");
    print("</font></p>\n<p>Command: $command</p>\n");
    print("<p>&nbsp;</p>\n");
    print("<p>Please be sure to refresh your screen when you go back.</p>\n");

    // free the result and close the connection 
    pg_freeresult($r1);
    pg_close($c1);
?>
</body>
</html>
