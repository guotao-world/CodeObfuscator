#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <ctype.h>
#define FILE_MAX_SIZE   1600000

#define FILE_NUMBER_vcpjt	(0+0)
#define CONTENT_NUMBER	(146-41)
#define EDIT_NUMBER   (FILE_NUMBER_vcpjt+CONTENT_NUMBER)

#define DELETE_COMMENT   (1)   //删除 /* */
#define DELETE_COMMENT1  (1)   //删除 //
#define DELETE_COMMENT2  (1)   //删除 #if 0  #endif

#define MAX_FUNCTIONS 3000
#define MAX_NAME_LENGTH 100
#define MAX_CUSTOM_TYPES 1000   
#define MAX_TYPE_NAME_LENGTH 100
#define version_number   "1.1   2026.4.30"
int function_total_num = 0;
int variable_total_num = 0;  
int struct_total_num = 0;   
int custom_type_count = 0;   
int custom_func_whitelist_count = 0;   
int custom_file_whitelist_count = 0;
int custom_decl_prefix_count = 0;
int custom_var_whitelist_count = 0;
int fileCount = 0;
char filetxt[FILE_MAX_SIZE + 512];
char* modifle[5000][3];  
#define DIASBLE_RENAME_FILE   0
char identify_types[100];
char* global_struct_names[MAX_FUNCTIONS];

char custom_return_types[MAX_CUSTOM_TYPES][MAX_TYPE_NAME_LENGTH];
char custom_func_whitelist[MAX_CUSTOM_TYPES][MAX_TYPE_NAME_LENGTH];
char custom_file_whitelist[MAX_CUSTOM_TYPES][MAX_TYPE_NAME_LENGTH];
char custom_decl_prefixes[MAX_CUSTOM_TYPES][MAX_TYPE_NAME_LENGTH];
char custom_var_whitelist[MAX_CUSTOM_TYPES][MAX_TYPE_NAME_LENGTH];

FILE* compare_file;




typedef enum {
	SECTION_NONE,
	SECTION_FILE_WHITELIST,
	SECTION_FUNC_WHITELIST,
	SECTION_CUSTOM_TYPES,
	SECTION_DECL_PREFIXES,
	SECTION_VAR_WHITELIST
} ConfigSection;

// 读取配置文件
int load_whitelist_config(const char* config_path) {
	if (config_path == NULL || strlen(config_path) == 0) {
		printf("错误：配置文件路径为空\n");
		return -1;
	}

	// 打开配置文件
	FILE* fp = fopen(config_path, "r");
	if (fp == NULL) {
		printf("错误：无法打开配置文件 %s (错误码: %d)\n", config_path, GetLastError());
		return -2;
	}
	printf("开始读取配置文件：%s\n", config_path);

	char line_buf[512];  // 行缓冲区
	ConfigSection current_section = SECTION_NONE;
	int line_num = 0;

	while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
		line_num++;
		char* p = line_buf;

		// 1. 清理行内容：去除首尾空格/制表符
		while (*p && isspace((unsigned char)*p)) p++;  // 跳过行首空白
		size_t len = strlen(p);
		while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' || isspace((unsigned char)p[len - 1]))) {
			p[--len] = '\0';  // 去除行尾换行/回车/空格
		}

		// 2. 跳过空行和注释行
		if (len == 0 || *p == '#') {
			continue;
		}

		// 3. 识别配置段（新增[CustomTypes]段识别）
		if (*p == '[' && p[len - 1] == ']') {
			// 提取段名（去掉[]）
			char section_name[64] = { 0 };
			strncpy_s(section_name, sizeof(section_name) - 1, p + 1, len - 2);

			// 判断当前段类型（新增CustomTypes判断）
			if (_stricmp(section_name, "FileWhitelist") == 0) {
				current_section = SECTION_FILE_WHITELIST;
				printf("  开始解析【文件白名单】段\n");
			}
			else if (_stricmp(section_name, "FuncWhitelist") == 0) {
				current_section = SECTION_FUNC_WHITELIST;
				printf("  开始解析【函数白名单】段\n");
			}
			else if (_stricmp(section_name, "CustomTypes") == 0) {  // 新增
				current_section = SECTION_CUSTOM_TYPES;
				printf("  开始解析【自定义类型】段\n");
			}
			else if (_stricmp(section_name, "DeclPrefixes") == 0) {  // 新增：声明前缀段
				current_section = SECTION_DECL_PREFIXES;
				printf("  开始解析【自定义声明前缀】段\n");
			}
			else if (_stricmp(section_name, "VarWhitelist") == 0) {  // 新增：变量保护名单段
				current_section = SECTION_VAR_WHITELIST;
				printf("  开始解析【变量保护名单】段\n");
			}
			else {
				printf("警告：第%d行未知配置段 [%s]，跳过\n", line_num, section_name);
				current_section = SECTION_NONE;
			}
			continue;
		}

		// 4. 根据当前段解析内容（新增CustomTypes处理逻辑）
		switch (current_section) {
		case SECTION_FILE_WHITELIST:
			// 原有文件白名单解析逻辑（不变）
			if (custom_file_whitelist_count >= MAX_CUSTOM_TYPES) {
				printf("警告：文件白名单数组已达上限(%d)，跳过第%d行：%s\n",
					MAX_CUSTOM_TYPES, line_num, p);
				break;
			}
			strcpy_s(custom_file_whitelist[custom_file_whitelist_count], MAX_TYPE_NAME_LENGTH, p);
			printf("    新增文件白名单：%s\n", custom_file_whitelist[custom_file_whitelist_count]);
			custom_file_whitelist_count++;
			break;

		case SECTION_FUNC_WHITELIST:
			// 原有函数白名单解析逻辑（不变）
			if (custom_func_whitelist_count >= MAX_CUSTOM_TYPES) {
				printf("警告：函数白名单数组已达上限(%d)，跳过第%d行：%s\n",
					MAX_CUSTOM_TYPES, line_num, p);
				break;
			}
			strcpy_s(custom_func_whitelist[custom_func_whitelist_count], MAX_TYPE_NAME_LENGTH, p);
			printf("    新增函数白名单：%s\n", custom_func_whitelist[custom_func_whitelist_count]);
			custom_func_whitelist_count++;
			break;

		case SECTION_CUSTOM_TYPES:  // 新增：自定义类型解析逻辑
			// 检查自定义类型数组边界
			if (custom_type_count >= MAX_CUSTOM_TYPES) {
				printf("警告：自定义类型数组已达上限(%d)，跳过第%d行：%s\n",
					MAX_CUSTOM_TYPES, line_num, p);
				break;
			}
			// 存入自定义类型数组
			strcpy_s(custom_return_types[custom_type_count], MAX_TYPE_NAME_LENGTH, p);
			printf("    新增自定义类型：%s\n", custom_return_types[custom_type_count]);
			custom_type_count++;
			break;

		case SECTION_DECL_PREFIXES:  // 新增：自定义声明前缀解析逻辑
			// 检查自定义前缀数组边界
			if (custom_decl_prefix_count >= MAX_CUSTOM_TYPES) {
				printf("警告：自定义前缀数组已达上限(%d)，跳过第%d行：%s\n",
					MAX_CUSTOM_TYPES, line_num, p);
				break;
			}
			// 存入自定义前缀数组
			strcpy_s(custom_decl_prefixes[custom_decl_prefix_count], MAX_TYPE_NAME_LENGTH, p);
			printf("    新增自定义前缀：%s\n", custom_decl_prefixes[custom_decl_prefix_count]);
			custom_decl_prefix_count++;
			break;

		case SECTION_VAR_WHITELIST:  // 新增：变量保护名单解析逻辑
			// 检查数组边界
			if (custom_var_whitelist_count >= MAX_CUSTOM_TYPES) {
				printf("警告：变量保护名单数组已达上限(%d)，跳过第%d行：%s\n",
					MAX_CUSTOM_TYPES, line_num, p);
				break;
			}
			// 存入保护名单
			strcpy_s(custom_var_whitelist[custom_var_whitelist_count], MAX_TYPE_NAME_LENGTH, p);
			printf("    新增保护变量：%s\n", custom_var_whitelist[custom_var_whitelist_count]);
			custom_var_whitelist_count++;
			break;

		default:
			printf("警告：第%d行内容不属于任何有效段，跳过：%s\n", line_num, p);
			break;
		}
	}

	// 关闭文件并返回结果
	fclose(fp);
	printf("配置文件读取完成！\n");
	printf("  - 解析自定义类型：%d 个\n", custom_type_count);    // 新增
	printf("  - 解析文件白名单：%d 个\n", custom_file_whitelist_count);
	printf("  - 解析函数白名单：%d 个\n", custom_func_whitelist_count);
	printf("  - 解析自定义前缀：%d 个\n", custom_decl_prefix_count);
	printf("  - 解析变量保护名单：%d 个\n", custom_var_whitelist_count);
	return 0;
}




void ask_for_custom_types() {
	char input[256];
	char another = 'y';

	printf("==================== 自定义类型输入 ====================\n");
	printf("您是否有自定义的变量类型需要重命名？(y/n): ");
	fgets(input, sizeof(input), stdin);

	if (input[0] == 'y' || input[0] == 'Y') {
		while (another == 'y' || another == 'Y') {
			if (custom_type_count >= MAX_CUSTOM_TYPES) {
				printf("已达到最大自定义类型数量 (%d)\n", MAX_CUSTOM_TYPES);
				break;
			}

			printf("\n请输入自定义类型名称 (例如: int, char): ");
			fgets(custom_return_types[custom_type_count], MAX_TYPE_NAME_LENGTH, stdin);

			// 去除换行符
			size_t len = strlen(custom_return_types[custom_type_count]);
			if (len > 0 && custom_return_types[custom_type_count][len - 1] == '\n') {
				custom_return_types[custom_type_count][len - 1] = '\0';
			}

			// 检查是否为空
			if (strlen(custom_return_types[custom_type_count]) == 0) {
				printf("类型名称不能为空，跳过此项。\n");
				continue;
			}

			printf("已添加自定义类型: %s\n", custom_return_types[custom_type_count]);
			custom_type_count++;

			printf("是否继续添加自定义类型？(y/n): ");
			fgets(input, sizeof(input), stdin);
			another = input[0];
		}
	}

	printf("已添加 %d 个自定义类型。\n", custom_type_count);
	printf("======================================================\n\n");
}

void ask_for_func_whitelist() {
	char input[256];
	char another = 'y';

	printf("==================== 白名单函数输入 ====================\n");
	printf("您是否有白名单函数？(y/n): ");
	fgets(input, sizeof(input), stdin);

	if (input[0] == 'y' || input[0] == 'Y') {
		while (another == 'y' || another == 'Y') {
			if (custom_func_whitelist_count >= MAX_CUSTOM_TYPES) {
				printf("已达到最大白名单数量 (%d)\n", MAX_CUSTOM_TYPES);
				break;
			}

			printf("\n请输入白名单函数 (例如: Encoder,Decoder ): ");
			fgets(custom_func_whitelist[custom_func_whitelist_count], MAX_TYPE_NAME_LENGTH, stdin);

			// 去除换行符
			size_t len = strlen(custom_func_whitelist[custom_func_whitelist_count]);
			if (len > 0 && custom_func_whitelist[custom_func_whitelist_count][len - 1] == '\n') {
				custom_func_whitelist[custom_func_whitelist_count][len - 1] = '\0';
			}

			// 检查是否为空
			if (strlen(custom_func_whitelist[custom_func_whitelist_count]) == 0) {
				printf("类型名称不能为空，跳过此项。\n");
				continue;
			}

			printf("已添加白名单函数: %s\n", custom_func_whitelist[custom_func_whitelist_count]);
			custom_func_whitelist_count++;

			printf("是否继续添加白名单函数？(y/n): ");
			fgets(input, sizeof(input), stdin);
			another = input[0];
		}
	}

	printf("已添加 %d 个自定义类型。\n", custom_func_whitelist_count);
	printf("======================================================\n\n");
}
void ask_for_file_whitelist() {
	char input[256];
	char another = 'y';

	printf("==================== 白名单文件输入 ====================\n");
	printf("您是否有白名单文件？(y/n): ");
	fgets(input, sizeof(input), stdin);

	if (input[0] == 'y' || input[0] == 'Y') {
		while (another == 'y' || another == 'Y') {
			if (custom_file_whitelist_count >= MAX_CUSTOM_TYPES) {
				printf("已达到最大白名单数量 (%d)\n", MAX_CUSTOM_TYPES);
				break;
			}

			printf("\n请输入白名单文件 (例如: studio.h,abcd.c ): ");
			fgets(custom_file_whitelist[custom_file_whitelist_count], MAX_TYPE_NAME_LENGTH, stdin);

			// 去除换行符
			size_t len = strlen(custom_file_whitelist[custom_file_whitelist_count]);
			if (len > 0 && custom_file_whitelist[custom_file_whitelist_count][len - 1] == '\n') {
				custom_file_whitelist[custom_file_whitelist_count][len - 1] = '\0';
			}

			// 检查是否为空
			if (strlen(custom_file_whitelist[custom_file_whitelist_count]) == 0) {
				printf("类型名称不能为空，跳过此项。\n");
				continue;
			}

			printf("已添加白名单文件: %s\n", custom_file_whitelist[custom_file_whitelist_count]);
			custom_file_whitelist_count++;

			printf("是否继续添加白名单文件？(y/n): ");
			fgets(input, sizeof(input), stdin);
			another = input[0];
		}
	}

	printf("已添加 %d 个自定义类型。\n", custom_file_whitelist_count);
	printf("======================================================\n\n");
}

void File_scan_recursive(const char* path, const char* relative_path) {
	char searchPattern[MAX_PATH];
	WIN32_FIND_DATAA findData;
	HANDLE hFind;

	// 构建搜索模式
	if (path[0] == '\0' || strcmp(path, ".") == 0) {
		strcpy_s(searchPattern, MAX_PATH, "*.*");
	}
	else {
		sprintf_s(searchPattern, MAX_PATH, "%s\\*.*", path);
	}

	// 开始搜索
	hFind = FindFirstFileA(searchPattern, &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND) {
			printf("目录为空: %s\n", path);
		}
		else {
			printf("无法访问目录 '%s' (错误: %d)\n", path, err);
		}
		return;
	}

	do {

		// 跳过特殊目录
		if (strcmp(findData.cFileName, ".") == 0 ||
			strcmp(findData.cFileName, "..") == 0) {
			continue;
		}

		// 构建完整路径
		char full_path[MAX_PATH];
		sprintf_s(full_path, MAX_PATH, "%s\\%s", path, findData.cFileName);

		// 检查是文件还是目录
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// 如果是目录，递归扫描
			char new_relative_path[MAX_PATH];
			if (strlen(relative_path) == 0) {
				sprintf_s(new_relative_path, MAX_PATH, "%s", findData.cFileName);
			}
			else {
				sprintf_s(new_relative_path, MAX_PATH, "%s\\%s", relative_path, findData.cFileName);
			}
			File_scan_recursive(full_path, new_relative_path);
		}
		else {
			// 如果是文件，添加到文件列表
			if (fileCount >= 5000) {
				printf("文件数量超过限制 (5000)\n");
				break;
			}


			// 检查文件是否在白名单中
			int in_whitelist = 0;
			if (custom_file_whitelist_count > 0) {
				for (int j = 0; j < custom_file_whitelist_count; j++) {
					if (strcmp(findData.cFileName, custom_file_whitelist[j]) == 0) {
						in_whitelist = 1;
						printf("文件 %s 在白名单中，将完全跳过处理\n", findData.cFileName);
						break;
					}


				}
			}

			// 添加到文件列表
			modifle[fileCount][0] = _strdup(findData.cFileName);
			// 存储相对路径
			modifle[fileCount][2] = _strdup(relative_path);

			// ============== 修改：标记白名单文件 ==============
			if (in_whitelist) {
				// 白名单文件：使用特殊标记
				modifle[fileCount][1] = _strdup("WHITELIST_FILE");
			}
			else {
				// 非白名单文件：生成新文件名
				int length111 = strlen(modifle[fileCount][0]);
				char filerename_str[50];
				sprintf_s(filerename_str, sizeof(filerename_str), "filename%d%c%c",
					fileCount,
					modifle[fileCount][0][length111 - 2],
					modifle[fileCount][0][length111 - 1]);

				modifle[fileCount][1] = _strdup(filerename_str);
			}
			// ============== 修改结束 ==============

			fileCount++;
		}
	} while (FindNextFileA(hFind, &findData) != 0);

	FindClose(hFind);
}







// 检查是否是返回类型（修改后包含自定义类型）
int is_return_type(const char* word)
{
	memset(identify_types, 0, sizeof(identify_types));
	char struct_names[1000];
	// 先检查自定义类型
	for (int i = 0; i < custom_type_count; i++) {
		const char* w = word;
		while (isspace((unsigned char)*w)) w++;

		if (strncmp(w, custom_return_types[i], strlen(custom_return_types[i])) == 0) {
			int len = strlen(custom_return_types[i]);

			if (w[len] == '\0' || w[len] == ' ' || w[len] == '\t' ||
				w[len] == '*' || w[len] == '(') {
				strncpy(identify_types, custom_return_types[i], len);
				identify_types[len] = '\0';
				//printf("识别到类型: %s (来自配置)\n", identify_types);

				return 1;
			}
		}
	}
	// 检查全局结构体名称
	for (int i = 0; i < struct_total_num; i++) {
		if (global_struct_names[i] != NULL) {
			sprintf(struct_names, "struct %s\0", global_struct_names[i]);
			const char* w = word;
			while (isspace((unsigned char)*w)) w++;

			if (strncmp(w, global_struct_names[i], strlen(global_struct_names[i])) == 0){
				int len = strlen(global_struct_names[i]);
				//printf("%s\n", global_struct_names[i]);

				if (w[len] == '\0' || w[len] == ' ' || w[len] == '\t' ||
					w[len] == '*' || w[len] == '(') {
					strncpy(identify_types, global_struct_names[i], len);
					identify_types[len] = '\0';
					return 1;
				}
			}
			else if (strncmp(w, struct_names, strlen(struct_names)) == 0)  {
				int len = strlen(struct_names);
				//printf("%s\n", struct_names);

				if (w[len] == '\0' || w[len] == ' ' || w[len] == '\t' ||
					w[len] == '*' || w[len] == '(') {
					strncpy(identify_types, struct_names, len);
					identify_types[len] = '\0';
					return 1;
				}
			}
		}
	}



	return 0;
}

// 变量名识别
int extract_variable_name_enhanced(const char* line, char* variable_name) {
	char temp[5000];
	strcpy(temp, line);
	int i = 0, j;

	// 静态变量：记录当前处理行的位置（按行独立，避免跨行长状态污染）
	static char last_processed_line[1024] = "";
	static int current_pos = 0; // 当前处理到的位置

	// 重置条件：1.新行 2.处理完当前行（current_pos=0） 3.行内容变化
	if (strcmp(line, last_processed_line) != 0 || current_pos == 0) {
		strcpy(last_processed_line, line);
		current_pos = 0; // 重置到行首

		// 移除前导空白
		char* p = temp;
		while (isspace((unsigned char)*p)) p++;

		// 检查常见的前缀关键字（如 static, const, extern 等）
		const char* prefixes[] = { "static const","static volatile","extern const", "static", "const", "extern", "volatile", "register","auto", NULL };
		/* 循环剥前缀，支持 NVoC_DATA const 等多前缀组合 */
		int removed = 1;
		while (removed) {
			removed = 0;
			/* 先查自定义前缀（来自 [DeclPrefixes] 配置，如 NVoC_DATA 等宏前缀） */
			for (i = 0; i < custom_decl_prefix_count; i++) {
				int len = strlen(custom_decl_prefixes[i]);
				if (strncmp(p, custom_decl_prefixes[i], len) == 0 && (isspace((unsigned char)p[len]) || p[len] == '*')) {
					p += len;
					while (isspace((unsigned char)*p)) p++;
					removed = 1;
					break;
				}
			}
			/* 再查内置前缀 */
			if (!removed) {
				for (i = 0; prefixes[i] != NULL; i++) {
					int len = strlen(prefixes[i]);
					if (strncmp(p, prefixes[i], len) == 0 && (isspace((unsigned char)p[len]) || p[len] == '*')) {
						p += len;
						while (isspace((unsigned char)*p)) p++;
						removed = 1;
						break;
					}
				}
			}
		}

		// 检查是否以返回类型开头
		char type_buffer[100];
		i = 0;
		while (*p && *p != '*' && *p != '(' && *p != '[' && *p != ',' && *p != ';' && i < 99) {
			type_buffer[i++] = *p++;
		}
		type_buffer[i] = '\0';
		// 检查是否是有效的类型
		if (!is_return_type(type_buffer)) {
			// 处理指针类型（如 int*, char*）
			while (i > 0 && type_buffer[i - 1] == '*') {
				type_buffer[--i] = '\0';
				p--;
			}
			if (!is_return_type(type_buffer)) {
				current_pos = 0; // 重置
				return 0; // 不是有效的类型
			}
		}

		// 回退到类型起始位置，跳过整个类型
		p -= (i - strlen(identify_types));
		while (*p && (!isspace((unsigned char)*p) && *p != '*')) {
			p++;
		}

		// 跳过类型后的空白和指针符号
		while (isspace((unsigned char)*p) || *p == '*') {
			p++;
		}

		// 记录第一个变量的起始位置
		current_pos = (int)(p - temp);
	}

	// 从当前处理位置开始提取变量
	char* p = temp + current_pos;
	if ((*p == '('))
	{
		if (*(++p) != '*')
		{
			p--;
		}
	}


	// 跳过当前位置的空白、星号（处理如 int* a, *b; 场景）
	while (*p && (isspace((unsigned char)*p) || *p == '*')) {
		p++;
	}



	// 提取变量名（直到遇到分隔符：逗号、分号、等号、括号、空格等）
	i = 0;
	while (*p && i < MAX_NAME_LENGTH - 1) {
		// 终止条件：分隔符（非变量名合法字符）
		if (*p == ',' || *p == ';' || *p == '=' || *p == '[' ||
			*p == '\t' || isspace((unsigned char)*p)) {
			break;
		}
		// 变量名合法字符：字母、数字、下划线
		if (isalnum((unsigned char)*p) || *p == '_') {
			variable_name[i++] = *p++;
		}
		else {
			break; // 遇到非法字符，终止提取
		}
	}
	variable_name[i] = '\0';
	//if (bracket_flag)
	//	*p = ')';
	if (i > 0) {
		// 定位到当前标识符的结束位置
		char* after_name = p;
		// 跳过标识符后的空白（处理如 int func (int a) 的情况）
		while (*after_name && isspace((unsigned char)*after_name)) {
			after_name++;
		}
		// 如果标识符后紧跟 ( → 是函数名，不提取
		if (*after_name == '(') {
			current_pos = 0;
			return 0;
		}
	}
	// 无有效变量名
	if (i == 0) {
		current_pos = 0;
		return 0;
	}

	// 更新当前处理位置（指向当前变量的结束位置）
	current_pos = (int)(p - temp);

	// 跳过当前变量后的内容（初始化、数组、括号等），定位到下一个变量
	while (*p && *p != ',' && *p != ';') {
		// 处理字符串字面量（避免误判引号内的逗号/分号）
		if (*p == '"' || *p == '\'') {
			char quote = *p;
			p++;
			while (*p && *p != quote) {
				if (*p == '\\') p++; // 跳过转义字符
				p++;
			}
			if (*p == quote) p++;
		}
		// 处理括号/大括号嵌套（如 int a = (1+2), b;）
		else if (*p == '(' || *p == '{' || *p == '[') {
			char open = *p;
			char close = (open == '(') ? ')' : (open == '{') ? '}' : ']';
			int level = 1;
			p++;
			while (*p && level > 0) {
				if (*p == open) level++;
				else if (*p == close) level--;
				p++;
			}
		}
		else {
			p++;
		}
	}

	// 定位到下一个变量的起始位置（跳过逗号/分号和空白）
	if (*p == ',') {
		p++; // 跳过逗号
		while (isspace((unsigned char)*p) || *p == '*') { // 跳过空白和指针符号
			p++;
		}
		current_pos = (int)(p - temp); // 更新位置，准备提取下一个变量
	}
	else {
		current_pos = 0; // 无更多变量，重置
	}

	// 验证变量名有效性
	if (strlen(variable_name) == 0) {
		current_pos = 0;
		return 0;
	}
	// 检查首字符是否合法（字母/下划线）
	if (!isalpha((unsigned char)variable_name[0]) && variable_name[0] != '_') {
		current_pos = 0;
		return 0;
	}
	// 检查后续字符是否合法（字母/数字/下划线）
	for (int k = 1; variable_name[k] != '\0'; k++) {
		if (!isalnum((unsigned char)variable_name[k]) && variable_name[k] != '_') {
			current_pos = 0;
			return 0;
		}
	}

	return 1; // 成功提取变量名
}
int detect_variables_from_text(const char* text, int text_size, char** variable_names) {
	// 将文本分割成行
	const char* lines[20000];
	int line_count = 0;

	const char* start = text;
	int line_length = 0;
	int brace_level = 0; // 大括号嵌套级别（用于排除函数体内的变量）

	for (int i = 0; i <= text_size; i++) {
		if (i == text_size || text[i] == '\n') {
			// 找到行结束
			if (line_length > 0) {
				// 分配内存存储这一行
				char* line_buf = (char*)malloc(line_length + 1);
				if (line_buf) {
					memcpy(line_buf, start, line_length);
					line_buf[line_length] = '\0';
					lines[line_count++] = line_buf;
				}
			}

			// 跳过可能的连续换行符
			while (i < text_size && (text[i] == '\n' || text[i] == '\r')) {
				i++;
			}

			// 设置下一行的开始
			start = text + i;
			line_length = 0;

			if (i >= text_size) break;
		}
		else {
			line_length++;
		}
	}

	// 处理最后一行（如果没有换行符）
	if (line_length > 0 && line_count < 20000) {
		char* line_buf = (char*)malloc(line_length + 1);
		if (line_buf) {
			memcpy(line_buf, start, line_length);
			line_buf[line_length] = '\0';
			lines[line_count++] = line_buf;
		}
	}

	// 检测变量名
	int variable_count = 0;

	for (int i = 0; i < line_count; i++) {
		const char* line = lines[i];

		/* skip preprocessor lines starting with # */
		{
			const char* pws = line;
			while (isspace((unsigned char)*pws)) pws++;
			if (*pws == '#') continue;
		}
		char variable_name[1000];

		// 更新大括号嵌套级别（简单检测）
		for (int j = 0; line[j] != '\0'; j++) {
			if (line[j] == '{') brace_level++;
			else if (line[j] == '}') brace_level--;
		}


		// 尝试提取变量名（可能一行有多个）
		int has_more_variables = 1;
		int attempts = 0;

		while (has_more_variables && (attempts < 10)) { // 限制尝试次数，避免无限循环
			attempts++;

			if (extract_variable_name_enhanced(line, variable_name)) {
				// 检查是否重复
				int duplicate = 0;
				for (int j = 0; j < variable_count; j++) {
					if (strcmp(variable_names[j], variable_name) == 0) {
						duplicate = 1;
						break;
					}
				}

				// 检查是否与C语言关键字冲突
				for (int k = 0;k < custom_type_count; k++) {
					if (strcmp(variable_name, custom_return_types[k]) == 0) {
						duplicate = 1;
						break;
					}
				}

				// 新增：检查是否与自定义类型名冲突
				for (int k = 0; k < custom_type_count; k++) {
					if (strcmp(variable_name, custom_return_types[k]) == 0) {
						duplicate = 1;
						break;
					}
				}

				// 检查是否命中变量保护名单（[VarWhitelist] 配置），命中则不混淆
				int in_var_whitelist = 0;
				for (int k = 0; k < custom_var_whitelist_count; k++) {
					if (strcmp(variable_name, custom_var_whitelist[k]) == 0) {
						in_var_whitelist = 1;
						break;
					}
				}

				if (!duplicate && strlen(variable_name) > 0 && !in_var_whitelist) {
					variable_names[variable_count] = (char*)malloc(strlen(variable_name) + 1);
					if (variable_names[variable_count]) {
						strcpy(variable_names[variable_count], variable_name);
						variable_count++;
					}
				}
				//printf("提取到变量: %s\n", variable_name);

			}
			else {
				has_more_variables = 0; // 没有更多变量
			}
		}
	}

	// 释放行内存
	for (int i = 0; i < line_count; i++) {
		free((void*)lines[i]);
	}

	return variable_count;
}

// 提取函数名
int extract_function_name_enhanced(const char* line, char* function_name, char* param_buffer, int* param_count)
{
	char temp[1024];
	strcpy(temp, line);
	int i = 0;

	char* p = temp;
	while (isspace((unsigned char)*p)) p++;
	char return_type[100];
	/* skip leading unknown macro qualifiers (e.g. NVOC_TEXT, __attribute__((...))) */
	int skip_cnt = 0;
	while (skip_cnt < 4) {
		i = 0;
		char* p2 = p;
		while (*p2 && *p2 != '(' && *p2 != '*' && i < 99) {
			return_type[i++] = *p2++;
		}
		return_type[i] = '\0';

		if (is_return_type(return_type)) {
			p = p2;
			break;
		}

		/* skip this unknown word and try the next one */
		while (*p && !isspace((unsigned char)*p) && *p != '(' && *p != '*') p++;
		while (isspace((unsigned char)*p)) p++;
		skip_cnt++;
	}

	if (!is_return_type(return_type)) {
		return 0;
	}

	p -= i - strlen(identify_types);

	// 跳过空白
	while (isspace((unsigned char)*p)) p++;
	if ((*p == '('))
	{
		if (*(++p) != '*')
		{
			p--;
		}
	}
	// 跳过可能的星号（指针返回）
	while (*p == '*') p++;
	while (isspace((unsigned char)*p)) p++;

	if (!isalpha((unsigned char)*p) && *p != '_') {
		return 0;
	}

	// 提取函数名直到遇到左括号或空白
	i = 0;
	while (*p && *p != '(' && !isspace((unsigned char)*p) && i < MAX_NAME_LENGTH - 1) {
		function_name[i++] = *p++;
	}
	if (function_name[i-1] == ')')
	{
		function_name[i-1] = '\0';
	}
	else
	function_name[i] = '\0';

	if (strchr(function_name, '[') != NULL) {
		return 0; // 包含数组括号，不是函数
	}

	// 验证后面是否有左括号（必须是 '('，不能是 '['）
	while (*p && isspace((unsigned char)*p)) p++;
	if (*p != '(') {  // 这里只检查 '('，不检查 '['
		return 0;
	}


	// ========== 新增：提取函数参数 ==========
	p++; // 跳过左括号
	*param_count = 0;
	char param_name[MAX_NAME_LENGTH];
	char temp_param[5000];
	int param_idx = 0;

	// 复制参数部分到临时缓冲区
	while (*p && *p != ')' && param_idx < 1023) {
		temp_param[param_idx++] = *p++;
	}
	temp_param[param_idx] = '\0';

	// 分割参数（按逗号分割）
	char* param_token = strtok(temp_param, ",");
	while (param_token != NULL && *param_count < MAX_FUNCTIONS) {
		// 清理参数token的前后空白
		while (isspace((unsigned char)*param_token)) param_token++;
		char* param_end = param_token + strlen(param_token) - 1;
		while (param_end >= param_token && isspace((unsigned char)*param_end)) {
			*param_end = '\0';
			param_end--;
		}

		if (strlen(param_token) == 0) {
			param_token = strtok(NULL, ",");
			continue;
		}

		// 提取参数名（从后往前找最后一个空格后的标识符）
		char* last_space = strrchr(param_token, ' ');
		if (last_space != NULL) {
			// 跳过空格，指向参数名开始
			char* param_start = last_space + 1;
			// 跳过可能的星号（如 int* a 中的*）
			while (*param_start == '*' || isspace((unsigned char)*param_start)) {
				param_start++;
			}

			// 提取参数名（直到非字母数字/下划线）
			i = 0;
			while (*param_start && (isalnum((unsigned char)*param_start) || *param_start == '_') && i < MAX_NAME_LENGTH - 1) {
				param_name[i++] = *param_start++;
			}
			param_name[i] = '\0';

			// 有效参数名才加入
			if (i > 0) {
				if (!isalpha((unsigned char)param_name[0]) && param_name[0] != '_') {
					param_token = strtok(NULL, ",");
					continue;
				}

				int is_keyword = 0;
				for (int k = 0; k < custom_type_count; k++) {
					if (strcmp(param_name, custom_return_types[k]) == 0) {
						is_keyword = 1;
						break;
					}
				}

				// 新增：检查是否是自定义类型
				for (int k = 0; k < custom_type_count; k++) {
					if (strcmp(param_name, custom_return_types[k]) == 0) {
						is_keyword = 1;
						break;
					}
				}

				if (!is_keyword) {
					// 将参数名存入缓冲区
					strcpy(param_buffer + (*param_count) * MAX_NAME_LENGTH, param_name);
					(*param_count)++;
				}
			}
		}

		param_token = strtok(NULL, ",");
	}
	// ========== 提取参数结束 ==========

	return 1;
}

// 函数检测
int detect_functions_enhanced(const char** code_lines, int num_lines,
	char** function_names, char** param_names, int* total_params) {
	int function_count = 0;
	*total_params = 0;

	for (int i = 0; i < num_lines; i++) {
		const char* line = code_lines[i];
		char function_name[MAX_NAME_LENGTH];
		char param_buffer[MAX_FUNCTIONS * MAX_NAME_LENGTH] = { 0 };
		int param_count = 0;

		/* skip preprocessor lines starting with # */
		{
			const char* pws = line;
			while (isspace((unsigned char)*pws)) pws++;
			if (*pws == '#') continue;
		}

		// 尝试提取函数名和参数
		if (extract_function_name_enhanced(line, function_name, param_buffer, &param_count)) {
			// 检查函数名是否重复
			int duplicate = 0;
			for (int j = 0; j < function_count; j++) {
				if (strcmp(function_names[j], function_name) == 0) {
					duplicate = 1;
					break;
				}
			}

			for (int j = 0; j < custom_func_whitelist_count; j++) {
				if (strcmp(custom_func_whitelist[j], function_name) == 0) {
					duplicate = 1;
					break;
				}
			}
			if (!duplicate && strlen(function_name) > 0) {
				function_names[function_count] = (char*)malloc(strlen(function_name) + 1);
				if (function_names[function_count]) {
					strcpy(function_names[function_count], function_name);
					function_count++;
				}
			}

			// 处理提取到的参数
			for (int p = 0; p < param_count && *total_params < MAX_FUNCTIONS; p++) {
				char* param_name = param_buffer + p * MAX_NAME_LENGTH;
				if (strlen(param_name) == 0) continue;

				// 检查参数名是否重复
				int param_duplicate = 0;
				for (int j = 0; j < *total_params; j++) {
					if (strcmp(param_names[j], param_name) == 0) {
						param_duplicate = 1;
						break;
					}
				}

				if (!param_duplicate) {
					param_names[*total_params] = (char*)malloc(strlen(param_name) + 1);
					if (param_names[*total_params]) {
						strcpy(param_names[*total_params], param_name);
						(*total_params)++;
					}
				}
			}
		}
	}

	return function_count;
}

int detect_functions_from_text(const char* text, int text_size, char** function_names, char** param_names, int* total_params) {
	// 将文本分割成行
	const char* lines[20000];  // 假设最多20000行
	int line_count = 0;

	const char* start = text;
	int line_length = 0;

	for (int i = 0; i <= text_size; i++) {
		if (i == text_size || text[i] == '\n') {
			// 找到行结束
			if (line_length > 0) {
				// 分配内存存储这一行
				char* line_buf = (char*)malloc(line_length + 1);
				if (line_buf) {
					memcpy(line_buf, start, line_length);
					line_buf[line_length] = '\0';
					lines[line_count++] = line_buf;
				}
			}

			// 跳过可能的连续换行符
			while (i < text_size && (text[i] == '\n' || text[i] == '\r')) {
				i++;
			}

			// 设置下一行的开始
			start = text + i;
			line_length = 0;

			if (i >= text_size) break;
		}
		else {
			line_length++;
		}
	}

	// 处理最后一行（如果没有换行符）
	if (line_length > 0 && line_count < 20000) {
		char* line_buf = (char*)malloc(line_length + 1);
		if (line_buf) {
			memcpy(line_buf, start, line_length);
			line_buf[line_length] = '\0';
			lines[line_count++] = line_buf;
		}
	}

	// 调用检测函数（包含参数提取）
	int function_count = detect_functions_enhanced(lines, line_count, function_names, param_names, total_params);

	// 释放行内存
	for (int i = 0; i < line_count; i++) {
		free((void*)lines[i]);
	}

	return function_count;
}

// 提取typedef struct的结构体名称
int extract_struct_name(const char* text, int text_size, char** struct_names) {
	// 1. 将文本分割成行（保留原逻辑，修复内存分配安全）
	const char* lines[20000];
	int line_count = 0;

	const char* start = text;
	int line_length = 0;

	for (int i = 0; i <= text_size; i++) {
		if (i == text_size || text[i] == '\n') {
			if (line_length > 0 && line_count < 20000) { // 增加数组越界检查
				char* line_buf = (char*)malloc(line_length + 1);
				if (line_buf) {
					memcpy(line_buf, start, line_length);
					line_buf[line_length] = '\0';
					lines[line_count++] = line_buf;
				}
			}

			// 跳过换行符（兼容\r\n）
			while (i < text_size && (text[i] == '\n' || text[i] == '\r')) {
				i++;
			}

			start = text + i;
			line_length = 0;

			if (i >= text_size) break;
		}
		else {
			line_length++;
		}
	}

	// 处理最后一行
	if (line_length > 0 && line_count < 20000) {
		char* line_buf = (char*)malloc(line_length + 1);
		if (line_buf) {
			memcpy(line_buf, start, line_length);
			line_buf[line_length] = '\0';
			lines[line_count++] = line_buf;
		}
	}

	// 2. 重构结构体识别逻辑（全局跟踪状态）
	int struct_count = 0;
	// 全局状态变量（跨多行有效）
	int in_typedef_struct = 0;  // 是否在typedef struct块中
	int in_normal_struct = 0;   // 是否在普通struct块中
	int brace_level = 0;        // 全局大括号层级
	char temp_name[MAX_NAME_LENGTH] = { 0 }; // 临时存储名称

	for (int i = 0; i < line_count; i++) {
		const char* line = lines[i];

		/* skip preprocessor lines starting with # */
		{
			const char* pws = line;
			while (isspace((unsigned char)*pws)) pws++;
			if (*pws == '#') continue;
		}

		// 动态分配行缓冲区（避免1024长度限制）
		char* temp_line = (char*)malloc(strlen(line) + 1);
		if (!temp_line) continue;
		strcpy(temp_line, line);

		// 移除行内注释
		char* comment = strstr(temp_line, "//");
		if (comment) *comment = '\0';

		// 跳过空白行
		char* p = temp_line;
		while (isspace((unsigned char)*p)) p++;
		if (*p == '\0') {
			free(temp_line);
			continue;
		}

		// ========== 1. 识别typedef struct起始 ==========
		if (!in_typedef_struct && !in_normal_struct) {
			if (strstr(p, "typedef struct") == p || strstr(p, "typedef\tstruct") == p) {
				in_typedef_struct = 1;
				brace_level = 0;
				memset(temp_name, 0, sizeof(temp_name));

				// 提取typedef struct后的标签名（如typedef struct abcd { ... }）
				p += strlen("typedef struct");
				while (isspace((unsigned char)*p)) p++;

				// 如果有标签名（如abcd），先存储
				if (isalnum((unsigned char)*p) || *p == '_') {
					int name_idx = 0;
					while (*p && (isalnum((unsigned char)*p) || *p == '_') && name_idx < MAX_NAME_LENGTH - 1) {
						temp_name[name_idx++] = *p++;
					}
					temp_name[name_idx] = '\0';

					// 验证并存储标签名（去重+非关键字）
					if (name_idx > 0) {
						int is_keyword = 0;
						for (int k = 0;k < custom_type_count; k++) {
							if (strcmp(temp_name, custom_return_types[k]) == 0) {
								is_keyword = 1;
								break;
							}
						}
						if (!is_keyword) {
							int duplicate = 0;
							for (int k = 0; k < struct_count; k++) {
								if (strcmp(struct_names[k], temp_name) == 0) {
									duplicate = 1;
									break;
								}
							}
							if (!duplicate) {
								struct_names[struct_count] = (char*)malloc(strlen(temp_name) + 1);
								if (struct_names[struct_count]) {
									strcpy(struct_names[struct_count], temp_name);
									struct_count++;
								}
							}
						}
					}

					// 跳过标签名后的空白，继续找{
					while (isspace((unsigned char)*p)) p++;
				}
			}
		}

		// ========== 2. 识别普通struct起始 ==========
		if (!in_typedef_struct && !in_normal_struct) {
			if (strstr(p, "struct") == p) {
				in_normal_struct = 1;
				brace_level = 0;
				memset(temp_name, 0, sizeof(temp_name));

				// 提取普通struct的标签名（如struct structname0 { ... }）
				p += strlen("struct");
				while (isspace((unsigned char)*p)) p++;

				// 情况1：有标签名（如structname0）
				if (isalnum((unsigned char)*p) || *p == '_') {
					int name_idx = 0;
					while (*p && (isalnum((unsigned char)*p) || *p == '_') && name_idx < MAX_NAME_LENGTH - 1) {
						temp_name[name_idx++] = *p++;
					}
					temp_name[name_idx] = '\0';

					// 验证并存储标签名
					if (name_idx > 0) {
						int is_keyword = 0;
						for (int k = 0; k < custom_type_count; k++) {
							if (strcmp(temp_name, custom_return_types[k]) == 0) {
								is_keyword = 1;
								break;
							}
						}
						if (!is_keyword) {
							int duplicate = 0;
							for (int k = 0; k < struct_count; k++) {
								if (strcmp(struct_names[k], temp_name) == 0) {
									duplicate = 1;
									break;
								}
							}
							if (!duplicate) {
								struct_names[struct_count] = (char*)malloc(strlen(temp_name) + 1);
								if (struct_names[struct_count]) {
									strcpy(struct_names[struct_count], temp_name);
									struct_count++;
								}
							}
						}
					}

					// 跳过标签名后的空白
					while (isspace((unsigned char)*p)) p++;
				}
			}
		}

		// ========== 3. 全局统计大括号层级（跨多行） ==========
			if (in_typedef_struct || in_normal_struct) {
		for (char* q = temp_line; *q != '\0'; q++) {
			if (*q == '{') {
				brace_level++;
			}
			else if (*q == '}') {
				brace_level--;

				// 大括号闭合时，提取别名
				if (brace_level == 0) {
					// 提取}后的别名
					char* brace_end = q + 1;
					while (isspace((unsigned char)*brace_end)) brace_end++;

					if (isalnum((unsigned char)*brace_end) || *brace_end == '_') {
						int name_idx = 0;
						memset(temp_name, 0, sizeof(temp_name));
						while (*brace_end && (isalnum((unsigned char)*brace_end) || *brace_end == '_') && name_idx < MAX_NAME_LENGTH - 1) {
							temp_name[name_idx++] = *brace_end++;
						}
						temp_name[name_idx] = '\0';

						// 验证并存储别名
						if (name_idx > 0) {
							int is_keyword = 0;
							for (int k = 0; k < custom_type_count; k++) {
								if (strcmp(temp_name, custom_return_types[k]) == 0) {
									is_keyword = 1;
									break;
								}
							}
							if (!is_keyword) {
								int duplicate = 0;
								for (int k = 0; k < struct_count; k++) {
									if (strcmp(struct_names[k], temp_name) == 0) {
										duplicate = 1;
										break;
									}
								}
								if (!duplicate) {
									struct_names[struct_count] = (char*)malloc(strlen(temp_name) + 1);
									if (struct_names[struct_count]) {
										strcpy(struct_names[struct_count], temp_name);
										struct_count++;
									}
								}
							}
						}
					}

					// 重置状态
					in_typedef_struct = 0;
					in_normal_struct = 0;
					break; // 退出当前行的大括号遍历
				}
			}
		}
			}

		free(temp_line); // 释放动态分配的行缓冲区
	}

	// 释放行内存
	for (int i = 0; i < line_count; i++) {
		free((void*)lines[i]);
	}

	return struct_count;
}



void remove_comments(char* text, long int* text_size, int file_index) {
	long int jj, kk, ll;
	for (jj = 0; jj < *text_size; jj++) {

#if DELETE_COMMENT   //删除 /* */
		if (file_index >= FILE_NUMBER_vcpjt) {
			if ((text[jj] == '/') && (text[jj + 1] == '*')) {
				for (kk = jj + 2; kk < *text_size; kk++) {
					if ((text[kk] == '*') && (text[kk + 1] == '/')) {
						kk = kk + 2;
						if ((text[kk] == 0x0a) || (text[kk] == 0x0d)) {
							kk++;
							if ((text[kk + 1] == 0x0a) || (text[kk + 1] == 0x0d))
								kk++;
						}
						break;
					}
				}

				long int delteLen = kk - jj;
				/* replace */
				for (ll = jj; ll < *text_size - delteLen; ll++) {
					text[ll] = text[ll + delteLen];
				}
				*text_size = *text_size - delteLen;
				jj--; // 重新检查当前位置
			};
		};
#endif

#if DELETE_COMMENT1   //删除 //
		if (file_index >= FILE_NUMBER_vcpjt) {
			if ((jj + 1 < *text_size) && (text[jj] == '/') && (text[jj + 1] == '/')) {
				for (kk = jj + 2; kk < *text_size; kk++) {
					if ((text[kk] == 0x0a) || (text[kk] == 0x0d)) {
						kk = kk + 1;
						if ((text[kk + 1] == 0x0a) || (text[kk + 1] == 0x0d)) {
							kk++;
						}
						break;
					}
				}

				short delteLen = kk - jj;
				/* replace */
				for (ll = jj; ll < *text_size - delteLen; ll++) {
					text[ll] = text[ll + delteLen];
				}
				*text_size = *text_size - delteLen;
				jj--; // 重新检查当前位置
			};
		};
#endif
	}
	for (jj = 0; jj < *text_size; jj++) {
#if DELETE_COMMENT2   //删除 #if 0  #endif
		if (file_index >= FILE_NUMBER_vcpjt) {
			if ((jj + 4 < *text_size)&&(text[jj] == '#') && (text[jj + 1] == 'i') && (text[jj + 2] == 'f') &&
				(text[jj + 3] == ' ') && (text[jj + 4] == '0')) {
				short int oneif = 0;

				for (kk = jj + 2; kk < *text_size; kk++) {
					if ((text[kk] == '#') && (text[kk + 1] == 'i') && (text[kk + 2] == 'f')) {
						oneif++;
					}

					if ((text[kk] == '#') && (text[kk + 1] == 'e') && (text[kk + 2] == 'l') &&
						(text[kk + 3] == 's') && (text[kk + 4] == 'e')) {
						if (oneif == 0) {
							/* 结束，删除 */
							text[kk] = '#';
							text[kk + 1] = 'i';
							text[kk + 2] = 'f';
							text[kk + 3] = ' ';
							text[kk + 4] = '1';


							break;
						}
					}

					if ((kk + 5 < *text_size) && (text[kk] == '#') && (text[kk + 1] == 'e') && (text[kk + 2] == 'n') &&
						(text[kk + 3] == 'd') && (text[kk + 4] == 'i') && (text[kk + 5] == 'f')) {
						if (oneif == 0) {
							kk = kk + 6;
							break;
						}
						else {
							oneif--; //忽略内嵌的 #if #endif
						}
					}
				}

				short delteLen = kk - jj;
				/* replace */
				for (ll = jj; ll < *text_size - delteLen; ll++) {
					text[ll] = text[ll + delteLen];
				}
				*text_size = *text_size - delteLen;
				jj--; // 重新检查当前位置
			};
		};
#endif

	}
}
int add_func_to_whitelist(const char* func_name)
{
	if (func_name == NULL || strlen(func_name) == 0) {
		return 0;
	}

	// 去重
	for (int i = 0; i < custom_func_whitelist_count; i++) {
		if (strcmp(custom_func_whitelist[i], func_name) == 0) {
			return 0;
		}
	}

	if (custom_func_whitelist_count >= MAX_CUSTOM_TYPES) {
		printf("警告：函数白名单数组已达上限(%d)，无法加入：%s\n",
			MAX_CUSTOM_TYPES, func_name);
		return -1;
	}

	strcpy_s(custom_func_whitelist[custom_func_whitelist_count],
		MAX_TYPE_NAME_LENGTH,
		func_name);

	printf("    自动加入函数白名单：%s\n",
		custom_func_whitelist[custom_func_whitelist_count]);

	custom_func_whitelist_count++;
	return 1;
}

void scan_whitelist_file_functions(const char* file_path)
{
	FILE* fp = fopen(file_path, "rb");
	if (fp == NULL) {
		printf("无法打开白名单文件用于提取函数: %s\n", file_path);
		return;
	}

	long int totalsize = 0;
	size_t sizePer = 0;

	memset(filetxt, 0x55, sizeof(filetxt));

	while (1) {
		sizePer = fread(&filetxt[totalsize], 1, 512, fp);

		if (sizePer < 512) {
			totalsize += sizePer;
			break;
		}

		totalsize += sizePer;

		if (totalsize > FILE_MAX_SIZE) {
			printf("白名单文件过大，无法提取函数: %s\n", file_path);
			fclose(fp);
			return;
		}
	}

	fclose(fp);

	// 去注释，避免注释里的函数声明被识别
	remove_comments(filetxt, &totalsize, 0);

	char* function_names[MAX_FUNCTIONS];

	char* param_names[MAX_FUNCTIONS];
	int total_params = 0;

	int function_count = detect_functions_from_text(
		filetxt,
		totalsize,
		function_names,
		param_names,
		&total_params
	);

	printf("白名单文件 %s 中识别到 %d 个函数\n", file_path, function_count);

	for (int i = 0; i < function_count; i++) {
		add_func_to_whitelist(function_names[i]);
		free(function_names[i]);
	}

	for (int i = 0; i < total_params; i++) {
		free(param_names[i]);
	}
}

int main0(int argc, char* argv[], char nvocplusfileinPath[8][256], char nvocplusfileOutPath[8][256], int mainstep)
{
	char nvocplusfile[1024];
	char nvocplusfileOut[1024];
	char dir_path[MAX_PATH];
	FILE* filein, * fileout;
	long int i, j, k, l;
	long int sizePer;
	long int totalsize;
	int pathselect;
	int ret = 0;
	int bracket_inx;
	char file_str[100] = { 0 };
	compare_file = fopen("../compare_file.txt", "wb");







	for (i = 0; i < fileCount; i++) {
		if (strcmp(modifle[i][1], "WHITELIST_FILE") == 0) {
			printf("扫描白名单文件中的函数并加入函数白名单: %s\\%s\n",
				modifle[i][2] ? modifle[i][2] : "", modifle[i][0]);

			pathselect = 0;

			strcpy_s(nvocplusfile, sizeof(nvocplusfile), nvocplusfileinPath[pathselect]);

			if (modifle[i][2] && strlen(modifle[i][2]) > 0) {
				strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][2]);
				strcat_s(nvocplusfile, sizeof(nvocplusfile), "\\");
			}

			strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][0]);

			scan_whitelist_file_functions(nvocplusfile);

			continue;
		}

		printf("Scanning file %d/%d for functions and variables: %s\\%s\n",
			i + 1, fileCount, modifle[i][2] ? modifle[i][2] : "", modifle[i][0]);

		pathselect = 0;

		strcpy_s(nvocplusfile, sizeof(nvocplusfile), nvocplusfileinPath[pathselect]);
		if (modifle[i][2] && strlen(modifle[i][2]) > 0) {
			strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][2]);
			strcat_s(nvocplusfile, sizeof(nvocplusfile), "\\");
		}
		strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][0]);

		filein = fopen(nvocplusfile, "rb");
		if (filein == NULL) {
			printf("open file %s for scan fail..\n", nvocplusfile);
			continue;
		}

		totalsize = 0;
		memset(filetxt, 0x55, sizeof(filetxt));

		while (1) {
			sizePer = fread(&filetxt[totalsize], 1, 512, filein);
			if (sizePer < 512) {
				totalsize += sizePer;
				break;
			}
			totalsize += sizePer;
			if (totalsize > FILE_MAX_SIZE) {
				printf(" file is too big\n");
				fclose(filein);
				return 2;
			}
		}

		fclose(filein);

		printf("Removing comments for scanning... ");
		remove_comments(filetxt, &totalsize, i);
		printf("done.\n");

		char* struct_names[MAX_FUNCTIONS];
		int struct_count = extract_struct_name(filetxt, totalsize, struct_names);
		sprintf_s(file_str, sizeof(file_str), "Found %d structs in %s      %s\r\n", struct_count, modifle[i][0], modifle[i][1]);
		fwrite(file_str, strlen(file_str), 1, compare_file); 
		memset(file_str, 0, sizeof(file_str));
		fwrite("               \r\n", strlen("               \r\n"), 1, compare_file);

		int local_struct_added = 0;

		for (int struct_idx = 0; struct_idx < struct_count; struct_idx++) {
			int already_exists = 0;

			int total_existing = fileCount + function_total_num + variable_total_num + struct_total_num +
				local_struct_added;
			for (int b = 0; b < total_existing; b++) {
				if (b >= 5000) break;
				if (modifle[b][0] != NULL && strcmp(struct_names[struct_idx], modifle[b][0]) == 0) {
					already_exists = 1;
					break;
				}
			}

			if (!already_exists) {
				int modifle_idx = fileCount + function_total_num + variable_total_num + struct_total_num +
					local_struct_added;

				if (modifle_idx >= 5000) {
					printf("Warning: modifle array full, cannot add more structs\n");
					break;
				}

				modifle[modifle_idx][0] = _strdup(struct_names[struct_idx]);
				global_struct_names[struct_total_num + local_struct_added] = _strdup(struct_names[struct_idx]);
				char struct_rename_str[50];
				sprintf_s(struct_rename_str, sizeof(struct_rename_str),
					"structname%d", struct_total_num + local_struct_added);

				modifle[modifle_idx][1] = _strdup(struct_rename_str);

				sprintf_s(file_str, sizeof(file_str), "Added struct: %s -> %s\r\n", modifle[modifle_idx][0], modifle[modifle_idx][1]);
				fwrite(file_str, strlen(file_str), 1, compare_file);
				memset(file_str, 0, sizeof(file_str));
				struct_total_num++;
			}
		}

		char* function_names[MAX_FUNCTIONS];
		char* param_names[MAX_FUNCTIONS];  
		int local_param_count = 0;       
		int function_count = detect_functions_from_text(filetxt, totalsize, function_names, param_names, &local_param_count);
		sprintf_s(file_str, sizeof(file_str), "Found %d functions in %s    %s\r\n", function_count, modifle[i][0], modifle[i][1]);
		fwrite(file_str, strlen(file_str), 1, compare_file);
		memset(file_str, 0, sizeof(file_str));

		sprintf_s(file_str, sizeof(file_str), "Found %d parameters in %s   %s\r\n", local_param_count, modifle[i][0], modifle[i][1]);
		fwrite(file_str, strlen(file_str), 1, compare_file);
		memset(file_str, 0, sizeof(file_str));
		fwrite("               \r\n", strlen("               \r\n"), 1, compare_file);

		int local_func_added = 0;

		for (int func_idx = 0; func_idx < function_count; func_idx++) {
			int already_exists = 0;

			int check_bound = fileCount + function_total_num + variable_total_num + struct_total_num + local_func_added + local_struct_added;
			for (int b = 0; b < check_bound; b++) {
				if (b >= 5000) break; 
				if (modifle[b][0] != NULL && strcmp(function_names[func_idx], modifle[b][0]) == 0) {

					already_exists = 1;
					break;
				}
			}

			if (!already_exists) {
				int modifle_idx = fileCount + function_total_num + variable_total_num + struct_total_num + local_func_added + local_struct_added;

				if (modifle_idx >= 5000) {
					printf("Warning: modifle array full, cannot add more functions/variables\n");
					break;
				}

				modifle[modifle_idx][0] = _strdup(function_names[func_idx]);

				char function_rename_str[50];


					sprintf_s(function_rename_str, sizeof(function_rename_str),
						"functionname%d", function_total_num + local_func_added);

				
				sprintf_s(function_rename_str, sizeof(function_rename_str),
					"functionname%d", function_total_num + local_func_added);
				modifle[modifle_idx][1] = _strdup(function_rename_str);

				sprintf_s(file_str, sizeof(file_str), "Added function: %s -> %s\r\n", modifle[modifle_idx][0], modifle[modifle_idx][1]);
				fwrite(file_str, strlen(file_str), 1, compare_file);
				memset(file_str, 0, sizeof(file_str));
				local_func_added++;
			}
		}

		char* variable_names[MAX_FUNCTIONS * 2]; 
		int variable_count = detect_variables_from_text(filetxt, totalsize, variable_names);
		sprintf_s(file_str, sizeof(file_str), "Found %d variables in %s     %s\r\n", variable_count, modifle[i][0], modifle[i][1]);
		fwrite(file_str, strlen(file_str), 1, compare_file);
		memset(file_str, 0, sizeof(file_str));
		fwrite("               \r\n", strlen("               \r\n"), 1, compare_file);
		int local_var_added = 0;

		for (int var_idx = 0; var_idx < variable_count; var_idx++) {
			int already_exists = 0;

			int total_existing = fileCount + function_total_num + variable_total_num + struct_total_num + local_func_added + local_var_added + local_struct_added;
			for (int b = 0; b < total_existing; b++) {
				if (b >= 5000) break; 
				if (modifle[b][0] != NULL && strcmp(variable_names[var_idx], modifle[b][0]) == 0) {
					already_exists = 1;
					break;
				}
			}

			if (!already_exists) {
				int modifle_idx = fileCount + function_total_num + variable_total_num + struct_total_num + local_func_added + local_var_added + local_struct_added;

				if (modifle_idx >= 5000) {
					printf("Warning: modifle array full, cannot add more functions/variables\n");
					break;
				}

				modifle[modifle_idx][0] = _strdup(variable_names[var_idx]);

				char variable_rename_str[50];
				sprintf_s(variable_rename_str, sizeof(variable_rename_str),
					"variablename%d", variable_total_num + local_var_added);

				modifle[modifle_idx][1] = _strdup(variable_rename_str);

				sprintf_s(file_str, sizeof(file_str), "Added variable: %s -> %s\r\n", modifle[modifle_idx][0], modifle[modifle_idx][1]);
				fwrite(file_str, strlen(file_str), 1, compare_file);
				memset(file_str, 0, sizeof(file_str));
				local_var_added++;
			}
		}

		int local_param_added = 0;
		for (int param_idx = 0; param_idx < local_param_count; param_idx++) {
			int already_exists = 0;

			int total_existing = fileCount + function_total_num + variable_total_num + struct_total_num +
				local_func_added + local_var_added + local_struct_added;
			for (int b = 0; b < total_existing; b++) {
				if (b >= 5000) break; 
				if (modifle[b][0] != NULL && strcmp(param_names[param_idx], modifle[b][0]) == 0) {
					already_exists = 1;
					break;
				}
			}

			if (!already_exists) {
				int modifle_idx = fileCount + function_total_num + variable_total_num + struct_total_num +
					local_func_added + local_var_added + local_struct_added;

				if (modifle_idx >= 5000) {
					printf("Warning: modifle array full, cannot add more parameters\n");
					break;
				}

				modifle[modifle_idx][0] = _strdup(param_names[param_idx]);
				char param_rename_str[50];
				sprintf_s(param_rename_str, sizeof(param_rename_str),
					"variablename%d", variable_total_num + local_var_added);

				modifle[modifle_idx][1] = _strdup(param_rename_str);

				sprintf_s(file_str, sizeof(file_str), "Added parameter: %s -> %s\r\n", modifle[modifle_idx][0], modifle[modifle_idx][1]);
				fwrite(file_str, strlen(file_str), 1, compare_file);
				memset(file_str, 0, sizeof(file_str));
				local_var_added++;
			}
		}

		function_total_num += local_func_added;
		variable_total_num += local_var_added;
		//struct_total_num += local_struct_added;

		for (int idx = 0; idx < function_count; idx++) {
			free(function_names[idx]);
		}
		for (int idx = 0; idx < variable_count; idx++) {
			free(variable_names[idx]);
		}
		for (int idx = 0; idx < local_param_count; idx++) {
			free(param_names[idx]);
		}
		for (int idx = 0; idx < struct_count; idx++) {
			free(struct_names[idx]);
		}
	}

	printf("\nTotal entries in modifle array:\n");
	int total_entries = fileCount + function_total_num + variable_total_num + struct_total_num;
	for (int idx = 0; idx < total_entries; idx++) {
		if (idx >= 5000) break;
		if (modifle[idx][0] != NULL) {
			printf("  [%d] %s -> %s\n", idx, modifle[idx][0], modifle[idx][1]);
		}
	}

	for (i = 0; i < fileCount; i++) {
		printf("\nProcessing file %d/%d\n", i + 1, fileCount);

		if (strcmp(modifle[i][1], "WHITELIST_FILE") == 0) {
			printf("Original: %s\\%s (白名单文件，直接复制)\n",
				modifle[i][2] ? modifle[i][2] : "", modifle[i][0]);

			pathselect = 0;

			strcpy_s(nvocplusfile, sizeof(nvocplusfile), nvocplusfileinPath[pathselect]);
			if (modifle[i][2] && strlen(modifle[i][2]) > 0) {
				strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][2]);
				strcat_s(nvocplusfile, sizeof(nvocplusfile), "\\");
			}
			strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][0]);

			strcpy_s(nvocplusfileOut, sizeof(nvocplusfileOut), nvocplusfileOutPath[pathselect]);

			if (modifle[i][2] && strlen(modifle[i][2]) > 0) {
				strcpy_s(dir_path, sizeof(dir_path), nvocplusfileOutPath[pathselect]);
				strcat_s(dir_path, sizeof(dir_path), modifle[i][2]);

				// 递归创建目录
				char* dir_ptr = dir_path;
				while (*dir_ptr) {
					if (*dir_ptr == '\\' || *dir_ptr == '/') {
						char temp = *dir_ptr;
						*dir_ptr = '\0';
						CreateDirectoryA(dir_path, NULL);
						*dir_ptr = temp;
					}
					dir_ptr++;
				}
				CreateDirectoryA(dir_path, NULL);

				strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), modifle[i][2]);
				strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), "\\");
			}

			strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), modifle[i][0]);

			// 直接复制文件
			filein = fopen(nvocplusfile, "rb");
			if (filein == NULL) {
				printf("无法打开输入文件: %s\n", nvocplusfile);
				continue;
			}

			fileout = fopen(nvocplusfileOut, "wb");
			if (fileout == NULL) {
				printf("无法创建输出文件: %s\n", nvocplusfileOut);
				fclose(filein);
				continue;
			}

			char buffer[4096];
			size_t bytes_read;
			while ((bytes_read = fread(buffer, 1, sizeof(buffer), filein)) > 0) {
				fwrite(buffer, 1, bytes_read, fileout);
			}

			fclose(filein);
			fclose(fileout);
			printf("已复制白名单文件: %s -> %s\n", nvocplusfile, nvocplusfileOut);
			continue;
		}

		printf("Original: %s\\%s\n", modifle[i][2] ? modifle[i][2] : "", modifle[i][0]);
		printf("Renamed: %s\\%s\n", modifle[i][2] ? modifle[i][2] : "", modifle[i][1]);

		pathselect = 0;

		strcpy_s(nvocplusfile, sizeof(nvocplusfile), nvocplusfileinPath[pathselect]);
		if (modifle[i][2] && strlen(modifle[i][2]) > 0) {
			strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][2]);
			strcat_s(nvocplusfile, sizeof(nvocplusfile), "\\");
		}
		strcat_s(nvocplusfile, sizeof(nvocplusfile), modifle[i][0]);

		strcpy_s(nvocplusfileOut, sizeof(nvocplusfileOut), nvocplusfileOutPath[pathselect]);

		if (modifle[i][2] && strlen(modifle[i][2]) > 0) {
			strcpy_s(dir_path, sizeof(dir_path), nvocplusfileOutPath[pathselect]);
			strcat_s(dir_path, sizeof(dir_path), modifle[i][2]);

			char* dir_ptr = dir_path;
			while (*dir_ptr) {
				if (*dir_ptr == '\\' || *dir_ptr == '/') {
					char temp = *dir_ptr;
					*dir_ptr = '\0';
					CreateDirectoryA(dir_path, NULL);
					*dir_ptr = temp;
				}
				dir_ptr++;
			}
			CreateDirectoryA(dir_path, NULL);

			strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), modifle[i][2]);
			strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), "\\");
		}

#if DIASBLE_RENAME_FILE
		strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), modifle[i][0]);
#else
		strcat_s(nvocplusfileOut, sizeof(nvocplusfileOut), modifle[i][1]);
#endif

		filein = fopen(nvocplusfile, "rb");
		printf("Opening file %s for read... ", nvocplusfile);
		if (filein == NULL) {
			printf("failed!\n");
			continue;
		}

		totalsize = 0;
		memset(filetxt, 0x55, sizeof(filetxt));

		while (1) {
			sizePer = fread(&filetxt[totalsize], 1, 512, filein);
			if (sizePer < 512) {
				totalsize += sizePer;
				break;
			}
			totalsize += sizePer;
			if (totalsize > FILE_MAX_SIZE) {
				printf(" file is too big\n");
				fclose(filein);
				return 2;
			}
		}

		fclose(filein);
		printf("done. File size: %d bytes\n", totalsize);

		printf("Removing comments... ");
		remove_comments(filetxt, &totalsize, i);
		printf("done.\n");

		printf("Modifying content... ");
		// 使用所有已收集的名称（文件名、函数名、变量名、参数名和结构体名）
		int mtimes = 0; 
		char* p1, * p2;
		long int m1;
		long int tempsize0, tempsizeNew;

		for (j = 0; j < totalsize; j++) {
			// 计算总的名称数：文件名 + 函数名 + 变量名 + 参数名 + 结构体名
			int total_names = fileCount + function_total_num + variable_total_num + struct_total_num;

			for (k = 0; k < total_names; k++) {
				// 跳过白名单文件的标记
				if (modifle[k][1] != NULL &&
					strcmp(modifle[k][1], "WHITELIST_FILE") == 0) {
					continue;
				}

				p1 = &filetxt[j];
				p2 = modifle[k][0];
				tempsize0 = strlen(modifle[k][0]);

				// 检查匹配（区分字符串内外的匹配）
				int match = 0;
				if (j > 0 && filetxt[j - 1] == '"')
				{
					for (m1 = 0; m1 < tempsize0; m1++) {
						if (tolower((unsigned char)p1[m1]) != tolower((unsigned char)p2[m1])) {
							break;
						}
					}
					if (m1 == tempsize0) match = 1;
				}
				else
				{
					for (m1 = 0; m1 < tempsize0; m1++) {
						if (p1[m1] != p2[m1]) {
							break;
						}
					}
					if (m1 == tempsize0) match = 1;
				}

				// 检查是否完全匹配
				if (match) {
					// 检查是否是完整的单词
					int is_complete_word = 1;

					if (j > 0) {
						char prev_char = filetxt[j - 1];
						if (isalnum((unsigned char)prev_char) || prev_char == '_') {
							is_complete_word = 0;
						}
					}

					if (j + tempsize0 < totalsize) {
						char next_char = filetxt[j + tempsize0];
						if (isalnum((unsigned char)next_char) || next_char == '_') {
							is_complete_word = 0;
						}
					}

					if (!is_complete_word) {
						continue;
					}

							/* combined preprocessor + string literal protection */
							{
								long int scan;
								int in_pp_line = 0;
								int is_include = 0;
								int is_define = 0;
								long int macro_name_start = -1, macro_name_end = -1;
								for (scan = j - 1; scan >= 0; scan--) {
									if (filetxt[scan] == '\n') {
										long int ppp = scan + 1;
										while (ppp < j && (filetxt[ppp] == ' ' || filetxt[ppp] == '\t')) ppp++;
										if (filetxt[ppp] == '#') {
											in_pp_line = 1;
											if (strncmp(filetxt + ppp, "#include", 8) == 0)
												is_include = 1;
											else if (strncmp(filetxt + ppp, "#define", 7) == 0) {
												is_define = 1;
												long int mp = ppp + 7;
												while (mp < j && (filetxt[mp] == ' ' || filetxt[mp] == '\t')) mp++;
												macro_name_start = mp;
												while (mp < j && (isalnum((unsigned char)filetxt[mp]) || filetxt[mp] == '_')) mp++;
												macro_name_end = mp;
											}
									}
									break;
								}
							}
									/* if no newline found, we are on first line: check if line starts with # */
									if (!in_pp_line && scan < 0 && filetxt[0] == '#') {
										in_pp_line = 1;
										if (strncmp(filetxt, "#include", 8) == 0) is_include = 1;
										else if (strncmp(filetxt, "#define", 7) == 0) {
											is_define = 1;
											long int mp = 0;
											while (mp < j && (filetxt[mp] == ' ' || filetxt[mp] == '\t' || filetxt[mp] == '#')) mp++;
											while (mp < j && (filetxt[mp] == ' ' || filetxt[mp] == '\t')) mp++;
											macro_name_start = mp;
											while (mp < j && (isalnum((unsigned char)filetxt[mp]) || filetxt[mp] == '_')) mp++;
											macro_name_end = mp;
										}
									}
								if (j == 0 && filetxt[0] == '#') {
								in_pp_line = 1;
								if (strncmp(filetxt, "#include", 8) == 0) is_include = 1;
								else if (strncmp(filetxt, "#define", 7) == 0) {
									is_define = 1;
									long int mp = 7;
									while (mp < j && (filetxt[mp] == ' ' || filetxt[mp] == '\t')) mp++;
									macro_name_start = mp;
									while (mp < j && (isalnum((unsigned char)filetxt[mp]) || filetxt[mp] == '_')) mp++;
									macro_name_end = mp;
								}
							}
								/* #include: allow everything (including filenames in quotes) */
								if (in_pp_line && !is_include) {
									if (is_define) {
										if (j >= macro_name_start && j < macro_name_end) continue;
									} else {
										/* 通用规则：#pragma 指令名(第一个标识符, ...) 的第一个标识符参与替换，
										   用于 #pragma DATA_SECTION(变量名, ".section") 等变量声明场景 */
										int allow_pp_replace = 0;
										long int pp_line_start = 0;
										long int pp_line_end = j;
										long int s3;
										for (s3 = j - 1; s3 >= 0; s3--) {
											if (filetxt[s3] == '\n') { pp_line_start = s3 + 1; break; }
										}
										while (pp_line_end < totalsize && filetxt[pp_line_end] != '\n') pp_line_end++;
										long int pp2 = pp_line_start;
										while (pp2 < pp_line_end && (filetxt[pp2] == ' ' || filetxt[pp2] == '\t')) pp2++;
										if (pp2 < pp_line_end && filetxt[pp2] == '#' && strncmp(filetxt + pp2, "#pragma", 7) == 0) {
											/* 跳过 #pragma 指令名 */
											long int pp = pp2 + 7;
											while (pp < pp_line_end && (filetxt[pp] == ' ' || filetxt[pp] == '\t')) pp++;
											while (pp < pp_line_end && (isalnum((unsigned char)filetxt[pp]) || filetxt[pp] == '_')) pp++;
											while (pp < pp_line_end && (filetxt[pp] == ' ' || filetxt[pp] == '\t')) pp++;
											/* 定位第一个括号参数内的标识符 [pp_start, pp_end) */
											if (pp < pp_line_end && filetxt[pp] == '(') {
												pp++;
												while (pp < pp_line_end && (filetxt[pp] == ' ' || filetxt[pp] == '\t')) pp++;
												long int pp_start = pp;
												while (pp < pp_line_end && (isalnum((unsigned char)filetxt[pp]) || filetxt[pp] == '_')) pp++;
												long int pp_end = pp;
												/* C 标识符必须以字母/下划线开头 */
												if (pp_start < pp_end && (isalpha((unsigned char)filetxt[pp_start]) || filetxt[pp_start] == '_')) {
													if (j >= pp_start && j < pp_end) allow_pp_replace = 1;
												}
											}
										}
										if (!allow_pp_replace) continue;
									}
								}
								/* skip string literals unless in #include */
								if (!is_include) {
									long int sscan;
									int quote_count = 0;
									for (sscan = j - 1; sscan >= 0; sscan--) {
										if (filetxt[sscan] == '"') {
											int backslash = 0;
											long int bs = sscan - 1;
											while (bs >= 0 && filetxt[bs] == '\\') { backslash++; bs--; }
											if (backslash % 2 == 0) quote_count++;
									}
								}
									if (quote_count % 2 == 1) continue;
								}
						}

					mtimes++;
					tempsizeNew = strlen(modifle[k][1]);

					// 开始替换
					if (tempsizeNew <= tempsize0) {
						p2 = modifle[k][1];
						for (m1 = 0; m1 < tempsizeNew; m1++) {
							p1[m1] = p2[m1];
						}
						totalsize = totalsize - (tempsize0 - tempsizeNew);
						j += tempsizeNew;
						p1 = &filetxt[0];
						for (m1 = j; m1 < totalsize; m1++) {
							p1[m1] = p1[m1 + (tempsize0 - tempsizeNew)];
						}
					}
					else {
						totalsize = totalsize - (tempsize0 - tempsizeNew);
						p1 = &filetxt[0];
						for (m1 = totalsize - 1; m1 >= j + tempsizeNew; m1--) {
							p1[m1] = p1[m1 + (tempsize0 - tempsizeNew)];
						}
						p2 = modifle[k][1];
						p1 = &filetxt[j];
						for (m1 = 0; m1 < tempsizeNew; m1++) {
							p1[m1] = p2[m1];
						}
						j += tempsizeNew;
					}
				}
			}
		}

		printf("done. %d replacements made.\n", mtimes);

		// 写入输出文件
		fileout = fopen(nvocplusfileOut, "wb");
		printf("Writing to %s... ", nvocplusfileOut);
		if (fileout == NULL) {
			printf("failed!\n");
			continue;
		}

		for (j = 0; j < totalsize; j++) {
			if (fwrite(&filetxt[j], 1, 1, fileout) == 0) {
				printf("write error at position %d\n", j);
				break;
			}
		}

		fclose(fileout);
		printf("done. New size: %d bytes\n", totalsize);
	}








	printf("\nTotal files processed: %d\n", fileCount);
	printf("Total functions collected: %d\n", function_total_num);
	printf("Total variables collected: %d\n", variable_total_num);
	printf("Total structs collected: %d\n", struct_total_num);  
	printf("Total custom types added: %d\n", custom_type_count); 
	fclose(compare_file);
	return 0;
}

int main(int argc, char* argv[]) {


	char config_path[MAX_PATH] = { 0 };
	int use_config_file = 0;
	printf("%s\n", version_number);



	if (argc >= 2) {
		strcpy_s(config_path, sizeof(config_path), argv[1]);
		use_config_file = 1;
		printf("【提示】命令行指定配置文件：%s\n", config_path);
	}
	else {
		char input_path[MAX_PATH] = { 0 };
		printf("==================== 配置文件选择 ====================\n");
		printf("请输入配置文件的完整路径（例如：user_whitelist_config.ini）：\n");
		printf("直接按回车将使用默认路径：..\\whitelist_config.ini\n");
		printf("请输入：");

		// 读取用户输入的配置文件路径
		fgets(input_path, sizeof(input_path), stdin);
		// 移除输入中的换行符
		input_path[strcspn(input_path, "\n")] = '\0';

		// 判断输入：有输入则用用户指定的路径，无输入则用默认路径
		if (strlen(input_path) > 0) {
			strcpy_s(config_path, sizeof(config_path), input_path);
			use_config_file = 1;
			printf("【提示】您选择的配置文件：%s\n", config_path);
		}
		else {
			// 使用默认路径
			strcpy_s(config_path, sizeof(config_path), "..\\whitelist_config.ini");
			use_config_file = 1;
			printf("【提示】未输入路径，使用默认配置文件：%s\n", config_path);
		}
	}

	int config_ret = load_whitelist_config(config_path);
	if (config_ret == 0) {
		printf("还有需要添加的吗？(y/n): ");
		char input[16];
		fgets(input, sizeof(input), stdin);
		if (input[0] == 'y' || input[0] == 'Y') {
			ask_for_custom_types();
			ask_for_func_whitelist();
			ask_for_file_whitelist();
		}


	}
	else if (config_ret != 0) {
		printf("\n【警告】配置文件读取失败！\n");
		printf("是否手动输入白名单/自定义类型？(y/n): ");
		char input[16];
		fgets(input, sizeof(input), stdin);
		if (input[0] == 'y' || input[0] == 'Y') {
			ask_for_custom_types();
			ask_for_func_whitelist();
			ask_for_file_whitelist();
		}
		else {
			printf("您选择退出程序，即将关闭...\n");
			exit(0);  
		}
	}

	printf("\n==================== 开始扫描文件 ====================\n");
	File_scan_recursive("..\\src_dir\\", "");

	int mainstep;
	char nvocplusfileinPath[8][256] = { "..\\src_dir\\" };
	char nvocplusfileOutPath0[8][256] = { "..\\dest_dir\\" };

	mainstep = 0;
	main0(argc, argv, nvocplusfileinPath, nvocplusfileOutPath0, mainstep);

	printf("\n==================== 程序执行完成 ====================\n");
	printf("按任意键退出...");
	return 0;
}
