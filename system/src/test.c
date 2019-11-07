#include <lpthread.h>
#include <unistd.h>
int foo1(){
	printf("%s\n", "Enter foo1");
	while(1){
		printf("%s\n", "Hola mundo");
		usleep(1000*1000);
	}
}

int foo2(){
	printf("%s\n", "Enter foo2");
	while(1){
		printf("%s\n", "Hola Jason");
		usleep(2000*1000);
	}
}

int main(){

	lpthread_t t1;
	lpthread_t t2;

	Lthread_create(&t1, NULL, &foo1, NULL);
	Lthread_create(&t2, NULL, &foo2, NULL);

	Lthread_join(t1, NULL);

}