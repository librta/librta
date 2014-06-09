<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
<html>
<!-- ----------------------------------------------------------------- -->
<!--  Run Time Access                                                  -->
<!--  Copyright (C) 2003-2006 Robert W Smith (bsmith@linuxtoys.org)    -->
<!--                                                                   -->
<!--   This program is distributed under the terms of the GNU          -->
<!--   LGPL.  See the file COPYING file.                               -->
<!-- ----------------------------------------------------------------- -->
<head>
<title>RTA Table Editor</title>
</head>
<body>
<center><h3>RTA Table Editor</h3></center>
<?php
    // Suppress Postgres error messages
    error_reporting(error_reporting() & 0xFFFD);

    // The user has selected a port for RTA access.  This is usually
    // unique to the RTA application.
    $port = htmlentities($_GET[port]);
    if (! $port)
        $port = 8888;    // In case it is not specified in URL.

    // connect to the database 
    $conn = pg_connect("localhost", "$port", "bsmith");
    if ($conn == "") { 
        printf("$s%s%s", "Unable to connect to application.<br>",
            "Please verify that the application is running and ",
            "listening on port $port.<br>");
        exit();
    }

    // Headings
    print("<table border=3 cellpadding=4 align=center width=65%>\n");
    print("<tr><th>Table Name</th><th>Description</th></tr>\n");

    // execute query 
    $command = "SELECT name, help, nrows FROM rta_tables";
    $result = pg_exec($conn, $command);
    if ($result == "") { 
        print("<p><font color=\"red\" size=+1>SQL Command failed!</p>");
        print("<p>Command: $command</p>\n");
        exit();
    }
    for($row = 0; $row < pg_NumRows($result); $row++)
    {
        $tblname  = pg_result($result, $row, 0);
        $tblhelp  = pg_result($result, $row, 1);
        $tblrows  = pg_result($result, $row, 2);
        print("<tr>\n<td><a href=rta_view.php?table=$tblname&offset=0");
        print("&nrows=$tblrows&port=$port>$tblname</a></td>\n");
        print("<td>$tblhelp</td></tr>\n");
    }

    print("</table>\n");

    // free the result and close the connection 
    pg_freeresult($result);
    pg_close($conn);
?>
</body>
</html>
