char *what_error(int no)
{
	char *str = "Unknow";

	switch (no) {
		case 0x01:
		    str = "Length Error";	
		    break;
		case 0x02:
		    str = "Initiator / Destination Error";
		    break;
		case 0x03:
		    str = "Command Type Error";
		    break;
		case 0x04:
		    str = "Command Code Error";
		    break;
		case 0x05:
		    str = "Parameters Error";
		    break;
		case 0x06:
		    str = "Checksum Error";
		    break;
	}
	return str;
}

