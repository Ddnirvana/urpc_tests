urpc: urpc_test.c urpc_lib.c
	gcc $^ -o $@ -static -pthread

clean:
	rm hello hello2 urpc -rf
