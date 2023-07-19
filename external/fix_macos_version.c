#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
		return -1;

	int foundIndex = 0, sdkIndex = 0;
	int c, oldVersion[] = { 0, 6, 10 }, newVersion[] = { 0, 13, 10 };
	FILE *fp = fopen(argv[1], "r+b");
	while ((c = fgetc(fp)) != EOF)
	{
		if (c == oldVersion[0])
			foundIndex = 1;
		else if (c != oldVersion[foundIndex])
			foundIndex = 0;
		else if ((++foundIndex == 3) && (((ftell(fp) - 3) % 4) == 0))
		{
			if (++sdkIndex == 2)
			{
				fseek(fp, -3, SEEK_CUR);
				for (int i = 0; i < 3; ++i)
					fputc(newVersion[i], fp);
				fclose(fp);
				return 0;
			}
			else
				foundIndex = 0;
		}
	}
	fclose(fp);
	return 0;
}
