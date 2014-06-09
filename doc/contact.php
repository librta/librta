<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 3.2//EN">
<html>
<head>
<title>rta Message Board</title>
</head>
<body>
<center><h3>rta Message Board</h3></center>
<?php
    $subject = htmlentities(current($_POST));
    $message = htmlentities(next($_POST));
    $to = "bob@mail";
    if (mail($to, $subject, $message))
    {
        print("<h3>Message sent. &nbsp; &nbsp; &nbsp; Thanks.</h3>\n");
    }
    else
    {
       echo("<p>Message delivery failed...</p>");
    }
?>
</body>
</html>
