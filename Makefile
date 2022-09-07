#NAME: Cao Xu
#EMAIL: cxcharlie@gmail.com
#ID: 704551688

default: thing
BB:=${shell uname -a | grep -o arvm7l}
thing:
ifeq ($(BB),arvm7l)
	gcc -Wall -Wextra -g -lm -lmraa lab4c_tcp.c -o lab4c_tcp
	gcc -Wall -Wextra -g -lm -lmraa -lssl -lcrypto lab4c_tls.c -o lab4c_tls
else
	gcc -Wall -Wextra -DDUMMY -lm -g lab4c_tcp.c -o lab4c_tcp
	gcc -Wall -Wextra -DDUMMY -lm -g -lssl -lcrypto lab4c_tls.c -o lab4c_tls
endif	

dist:
	tar -czvf lab4c-704551688.tar.gz lab4c_tls.c lab4c_tcp.c Makefile README

clean:
	rm -f lab4c_tcp lab4c_tls lab4c-704551688.tar.gz
