int _start()
{
  //void *vidmem = (void *)0x80000;
  void *vidmem = (void *)9000;
  ((char *)vidmem)[666] = 111;
  return 0;
}
