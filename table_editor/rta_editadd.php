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
<title>Edit or Add Row</title>
</head>
<body>
<?php
    // The user has selected a row in a table to edit.
    // We want to display the name, help, and value of
    // each field in the row.  If the row is editable,
    // we want to add it to a form which lets them 
    // change the value.
    $tbl  = htmlentities($_GET[table]);
    $row  = htmlentities($_GET[row]);
    $port = htmlentities($_GET[port]);

    // Say where we are.
    if ($row >= 0)
        print("<center><h3>Edit $tbl, row $row</h3></center>\n");
    else
        print("<center><h3>Insert New Row</h3></center>\n");

    // Suppress Postgres error messages
    error_reporting(error_reporting() & 0xFFFD);

    // connect to the database 
    $c1 = pg_connect("localhost", "$port", "bsmith");
    if ($c1 == "") { 
        printf("$s%s%s", "Unable to connect to application.<br>",
            "Please verify that the application is running and ",
            "listening on port $port.<br>");
        exit();
    }

    // Give URL for form processing (insert if row ==-1)
    if ($row >= 0)
        print("<form method=\"post\" action=rta_update.php>\n");
    else
        print("<form method=\"post\" action=rta_insert.php>\n");
    print("<td><input type=\"hidden\" name=\"__table\" value=\"$tbl\">\n");
    print("<td><input type=\"hidden\" name=\"__row\" value=\"$row\">\n");
    print("<td><input type=\"hidden\" name=\"__port\" value=\"$port\">\n");

    // print name, help, value of each column
    print("<center><table border=3 cellpadding=4 width=85%>\n");

    // execute query 
    $command = "SELECT name, flags, help, length, type FROM rta_columns WHERE table='$tbl'";
    $r1 = pg_exec($c1, $command);
    if ($r1 == "") { 
        print("<p><font color=\"red\" size=+1>SQL Command failed!</p>");
        print("<p>Command: $command</p>\n");
        exit();
    }

    print("<tr><th>Column</th><th width=20%>Value</th></tr>\n");
    print("</tr>\n");
    for($col = 0; $col < pg_NumRows($r1); $col++)
    {
        $colname   = pg_result($r1, $col, 0);
        $colflags  = pg_result($r1, $col, 1);
        $colhelp   = pg_result($r1, $col, 2);
        $collength = pg_result($r1, $col, 3);
        $coltype   = pg_result($r1, $col, 4);
        $colvalue = "";
        if (($coltype != 0) &&  // "0" is the type for strings,  See rta.h
            ($coltype != 4)) {  // "4" is the type for pointer to string.
            $collength = 20;
            $colvalue = 0;
        }

        // Get column value from table if an edit
        if ($row >= 0 ) {
            $command = "SELECT \"$colname\" FROM \"$tbl\" LIMIT 1 OFFSET $row";
            $r2 = pg_exec($c1, $command);
            if ($r2 == "") { 
                print("<p><font color=\"red\" size=+1>SQL Command failed!</p>");
                print("<p>Command: $command</p>\n");
                exit();
            }
            $colvalue  = pg_result($r2, 0, 0);
            pg_freeresult($r2);
        }

        // Display the column
        print("<tr><td><b>$colname</b><br>$colhelp</td>");
        if ($colflags & 2)  // "2" indicates a read-only field.  See rta.h
        {
            print("<td>$colvalue</td></tr>\n");
        }
        else
        {
            print("<td><input type=\"text\" name=\"$colname\" ");
            print("value=\"$colvalue\" maxlenght=$collength>");
            print("</td></tr>\n");
        }
    }

    print("</table></center>\n");
    print("<p align=right><input type=\"submit\"></p>\n");

    // free the result and close the connection 
    pg_freeresult($r1);
    pg_close($c1);
?>
</body>
</html>
