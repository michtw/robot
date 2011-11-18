ALL: cgic.c GetSsid.c
	gcc -Wall -static -o GetSsid.cgi cgic.c GetSsid.c
	gcc -Wall -static -o AutoDrive.cgi cgic.c AutoDrive.c
	gcc -Wall -static -o GetMovingStatus.cgi cgic.c GetMovingStatus.c
	gcc -Wall -static -o GetName.cgi cgic.c GetName.c
	gcc -Wall -static -o SetName.cgi cgic.c SetName.c
	gcc -Wall -static -o GetName-Json.cgi cgic.c GetName-Json.c
	gcc -Wall -static -o GetSsid-Json.cgi cgic.c GetSsid-Json.c
CLEAN:
	rm GetSsid.cgi AutoDrive.cgi GetMovingStatus.cgi GetName.cgi SetName.cgi
