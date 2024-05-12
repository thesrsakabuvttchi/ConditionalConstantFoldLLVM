int computeSquare(int x) {
    return x * x;
}

int computeTriple(int x) {
    return 3 * x;
}

int compute (int var)
{
  int result = 0;
  int a = 2;
  int b = 3;
  int c = 4 + a + b;
  
  float d = 2.0;
  float e = d+1.2+32.0;
  float f;

  if(e>1)
  {
	f =	e; 
  }
  else{
	f = 0;
  }

  result += a;
  result += b;
  result *= c;
  result /= 2;

	if(result==1000)
	{
		return(-1);
	}
	else
	{
		a = var;
	}

  result+=a;
  int inline_test1 = computeSquare(5);
  int inline_test2 = computeTriple(10);
  int use_inline = inline_test1+inline_test2;
  return result+use_inline;
}

int main()
{
  compute(9);
}