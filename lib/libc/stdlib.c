
int
atoi(const char *nptr)
{
    int i = 0;
    int val = 0;

    while (nptr[i] != '\0') {
	if (nptr[i] >= '0' && nptr[i] <= '9') {
	    val = val * 10 + (int)(nptr[i] - '0');
	} else {
	    return 0;
	}
	i++;
    }

    return val;
}

