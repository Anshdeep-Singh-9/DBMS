
#include "declaration.h"
#include <filesystem>

void drop(){
	char *tab;
	char *temp1;
	temp1=(char*)malloc(sizeof(char)*MAX_NAME);
	tab=(char*)malloc(sizeof(char)*MAX_NAME);
	cout<<"\nenter table name to delete: ";
	cin>>tab;
	strcpy(temp1,tab);
	int check=search_table(tab);
	if(check==0) printf("\n%s doesn't exist\n\n",tab);
	else if(check==1){
		std::filesystem::remove_all(std::filesystem::path("./table") / tab);

		std::ifstream in("./table/table_list");
		std::ofstream out("./table/table_list.tmp", std::ios::trunc);
		std::string table_name;
		while (in >> table_name) {
			if (table_name != tab) {
				out << table_name << '\n';
			}
		}
		in.close();
		out.close();
		std::filesystem::rename("./table/table_list.tmp", "./table/table_list");

        printf("\n%s deleted\n\n",temp1);
		printf("-------------------------------");
	}
	else{
		printf("\nerror\nexiting...\n\n");
	}
	free(temp1);
	free(tab);
}
