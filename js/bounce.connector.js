jQuery(document).ready(function() {
	jQuery("#edit-bounce-connector-protocol").change(function(e) {
	  var port = "";
	  switch(jQuery(this).val()) {
	    case "pop3":   port = 110; break;
	    case "pop3s":  port = 995; break;
	    case "imap":   port = 143; break;
	    case "imaps":  port = 993; break;
	  }
	  jQuery("#edit-bounce-connector-port").val(port);
	});
});