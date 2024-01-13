#include <stdio.h>
#include <syscall.h>

#define EXIT_SUCCESS 0

int
main (int argc, char **argv)
{
  int i;

  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}
