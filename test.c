int blub = 8;

int blub2[] = { 1,2,3,4,5,6,7,8,9 };


const char *blub3 = "hallooooooooooooooooo!";


int _start()
{
  //void *vidmem = (void *)0x80000;
  // void *vidmem = (void *)9000;

  // for (int i = 0; i < 100; i++)
  //   ((char *)vidmem)[i] = 111;
  
  // char x = ((char *)vidmem)[50];

  //char x = 111;

  // // if (x == 111)
  // // {
  // //   x = 50;
  // // }

  for (int i = 0; i < 9; i++)
  {
    blub = blub2[i];
    blub = blub3[i];
  }

  // if (x != 111)
  // {
  //   //x = 77;
  //   return 555;
  // }

  return 666;
}
