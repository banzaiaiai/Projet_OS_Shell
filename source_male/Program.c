#include <stdio.h>
int main(int argc, char *argv[]) {
  printf("Lancement\n");
  int is_ok = 1;
  while (is_ok)
  {
    /* code */
    char p;
    scanf("%c", &p);
    if(p == 'q'){
        is_ok = 0;
    }
  }
  printf("Fin du programe\n");
  return 0;
}