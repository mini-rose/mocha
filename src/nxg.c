#include <parser.h>

int main()
{
	token_list *list = tokens("./example.ff");

	parse(list);

	token_list_destroy(list);
	return 0;
}
