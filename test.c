int _start()
{
  //void *vidmem = (void *)0x80000;
  void *vidmem = (void *)8000; // lesbar machen
  ((char *)vidmem)[666] = 111;
  return 0;
}
