#include <PalmOS.h>
#include <68K\Hs.h>
#include <common\system\palmOneNavigator.h>
#include <common\system\HsKeyCodes.h>
#include <HsKeyTypes.h>

#include "PocketIME.h"
#include "PocketIME_Rsc.h"

//---------------------函数声明----------------------------------------------------------------
static void ShowStatus(UInt16 rscID, Char *chars, Int32 ms);
static WChar CharToLower(WChar key);
static void EnqueueResultToKey(Char *buf, UInt16 buflen);
static Boolean GetMBDetailInfo(stru_MBInfo *mb_info, Boolean ActivedOnly, Boolean mb_loaded);
static void SaveLoadMB(stru_MBInfo *mb_info, UInt8 op, Boolean show_status);
static void UnloadMB(UInt16 start, Boolean show_status);
static void SetMBInfoByNameType(Char *file_name, UInt32 db_type, Boolean inRAM, stru_MBInfo *mb_info);
static void GetMBInfoByNameType(Char *file_name, UInt32 db_type, Boolean inRAM, stru_MBInfo *mb_info);
static void SetMBInfoFormMBList(stru_MBInfo *mb_info, UInt16 mb_index);
static void GetMBInfoFormMBList(stru_MBInfo *mb_info, UInt16 mb_index, Boolean show_status, Boolean need_load_mb);
static Boolean hasOptionPressed(UInt16 modifiers, stru_Pref *pref);
static void SetCaretColor(Boolean set_default, stru_Pref *pref);
static void SetKeyRates(Boolean reset, stru_Pref *pref);
static void SetInitModeOfField(stru_Pref *pref);
static UInt8 GetInitModeOfField(stru_Pref *pref);

static void MainFormEventHandler(Boolean IsDA);
static void SetInitModeTrigger(Int16 mode, stru_Pref *pref);
static void MoveMBRecordInMBListDB(UInt16 record_index, ListType *lstP, Char ***mb_list, UInt8 direction);
static UInt16 UpdateMBListDB(char ***mb_list);
static UInt16 GetMBListInRAM(stru_MBList ***mb_list);
static UInt16 GetMBListOnVFS(stru_MBList ***mb_list_vfs);
static void AdvanceSettingEventHandler(stru_Pref *pref);
static WChar CustomKey(UInt8 kb_mode, stru_Pref *settingP);
static void SetBlurEventHandler(stru_Pref *pref, UInt16 mb_index);
static void SwitchBlurActiveStatus(stru_MBInfo *mb_info, UInt16 blur_num, UInt16 blur_index);
static void UpdateBlurList(ListType *lstP, stru_MBInfo *mb_info, Char ***blur_list, UInt16 *blur_num);
static Err SetFieldTextFromStr (FieldPtr field, Char *s, Boolean redraw);
UInt16 Make16BitRGBValue (UInt16 r, UInt16 g, UInt16 b);
static void * GetObjectPtr(FormPtr form, UInt16 objectID);
static void CreateWordEventHandler(Char *word, Char *key, stru_Pref *pref);

void SLWinDrawBitmap
(
        DmOpenRef dbP,        // (in)资源文件数据库指针
        UInt16 uwBitmapIndex, // (in)位图资源的Index或者ID
        Coord x,              // (in)位图左上角的x坐标
        Coord y,              // (in)位图左上角的y坐标
        Boolean bByIndex      // (in)true：根据资源索引来获取Bitmap
                              //     false：根据资源ID来获取Bitmap
                              // 如果为true，将忽略dbP参数
);

static FieldType *GetActiveField(stru_Pref *pref);
#pragma mark -
//////////////////////////////////////////////////////////////////////////////////////////////

//---------------------内核模块----------------------------------------------------------------
//初始化结果双向链表
static void InitResult(stru_Globe *globe)
{
	stru_Result		*result_ahead;
	
	if (globe->result_head.next != (void *)&globe->result_tail) //有节点，删除
	{
		globe->result = (stru_Result *)globe->result_head.next;
		while (globe->result != &globe->result_tail)
		{
			result_ahead = (stru_Result *)globe->result->prev;
			globe->result = (stru_Result *)globe->result->next;
			MemPtrFree(((stru_Result *)globe->result->prev)->result);
			MemPtrFree(globe->result->prev);
			result_ahead->next = (void *)globe->result;
			globe->result->prev = (void *)result_ahead;
		}
	}
	//修正当前节点
	globe->result = NULL;
}
//--------------------------------------------------------------------------
//新建结果链表节点
static void NewResult(stru_Globe *globe)
{
	stru_Result		*result_prev;
	stru_Result		*result_next;

	if (globe->result == NULL)
	{
		result_prev = &globe->result_head;
		result_next = &globe->result_tail;
	}
	else if (globe->result != &globe->result_tail)
	{
		result_prev = globe->result;
		result_next = (stru_Result *)globe->result->next;
	}
	else
	{
		result_prev = (stru_Result *)globe->result->prev;
		result_next = &globe->result_tail;
	}
	globe->result = (stru_Result *)MemPtrNew(stru_Result_length);
	MemSet(globe->result, stru_Result_length, 0x00);
	result_prev->next = (void *)globe->result;
	result_next->prev = (void *)globe->result;
	globe->result->prev = (void *)result_prev;
	globe->result->next = (void *)result_next;
}
//--------------------------------------------------------------------------
//初始化包含结果的记录的循环链表
static void InitMBRecord(stru_Globe *globe)
{
	stru_MBRecord		*mb_record_to_delete;
	stru_ContentOffset	*content;
	stru_ContentOffset	*content_to_delete;
	
	if (globe->mb_record_head != NULL)
	{
		//切断循环链表
		((stru_MBRecord *)globe->mb_record_head->prev)->next = NULL;
		//从表头开始删除所有节点
		globe->mb_record = (stru_MBRecord *)globe->mb_record_head;
		do
		{
			mb_record_to_delete = globe->mb_record;
			globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
			//删除偏移量链表
			content = (stru_ContentOffset *)mb_record_to_delete->offset_head.next;
			while (content != &mb_record_to_delete->offset_tail)
			{
				content_to_delete = content;
				content = (stru_ContentOffset *)content->next;
				MemPtrFree(content_to_delete);
			}
			//删除节点
			MemPtrFree(mb_record_to_delete);
		}while(globe->mb_record != NULL);
		//刷新节点
		globe->mb_record_head = NULL;
		globe->mb_record = NULL;
	}
}
//--------------------------------------------------------------------------
//新建包含结果的记录的循环链表的节点
static void NewMBRecord(stru_Globe *globe)
{
	stru_MBRecord	*new_mb_record;
	
	if (globe->mb_record_head != NULL) //不需要初始化
	{
		//分配新节点的内存
		new_mb_record = (stru_MBRecord *)MemPtrNew(stru_MBRecord_length);
		MemSet(new_mb_record, stru_MBRecord_length, 0x00);
		new_mb_record->offset_head.next = (void *)&new_mb_record->offset_tail;
		//修正指针
		new_mb_record->next = globe->mb_record->next;
		((stru_MBRecord *)globe->mb_record->next)->prev = (void *)new_mb_record;
		globe->mb_record->next = (void *)new_mb_record;
		new_mb_record->prev = (void *)globe->mb_record;
		//刷新当前节点
		globe->mb_record = new_mb_record;
	}
	else //初始化
	{
		//分配内存
		globe->mb_record_head = (stru_MBRecord *)MemPtrNew(stru_MBRecord_length);
		MemSet(globe->mb_record_head, stru_MBRecord_length, 0x00);
		globe->mb_record_head->offset_head.next = (void *)&globe->mb_record_head->offset_tail;
		//修正指针
		globe->mb_record_head->next = (void *)globe->mb_record_head;
		globe->mb_record_head->prev = (void *)globe->mb_record_head;
		//刷新当前节点
		globe->mb_record = globe->mb_record_head;
	}
}
//--------------------------------------------------------------------------
//删除包含结果的记录的循环链表的节点
static void DeleteMBRecord(stru_MBRecord *mb_record_to_delete, stru_Globe *globe)
{
	stru_ContentOffset	*content;
	stru_ContentOffset	*content_to_delete;
	
	if (mb_record_to_delete != globe->mb_record_head)
	{
		((stru_MBRecord *)(mb_record_to_delete->prev))->next = mb_record_to_delete->next;
		((stru_MBRecord *)(mb_record_to_delete->next))->prev = mb_record_to_delete->prev;
		//删除偏移量链表
		content = &mb_record_to_delete->offset_head;
		while (content->next != (void *)&mb_record_to_delete->offset_tail)
		{
			content_to_delete = content;
			content = (stru_ContentOffset *)content->next;
			MemPtrFree(content_to_delete);
		}
		MemPtrFree(mb_record_to_delete);
	}
	else
	{
		InitMBRecord(globe);
	}
}
//--------------------------------------------------------------------------
//根据情况，读取记录
static void DmGetRecordFromCardAndRAM(DmOpenRef db_ref, FileRef db_file_ref, UInt16 record_index, MemHandle *record_handle)
{
	(*record_handle) = NULL;
	
	if (db_ref != NULL)
	{
		(*record_handle) =  DmGetRecord(db_ref, record_index);
	}
	else
	{
		VFSFileDBGetRecord(db_file_ref, record_index, record_handle, NULL, NULL);
	}
	
	//return record_handle;
}
//--------------------------------------------------------------------------
//根据情况，释放记录
static void DmReleaseRecordFromCardAndRAM(DmOpenRef db_ref, UInt16 record_index, MemHandle *record_handle)
{
	MemHandleUnlock((*record_handle));
	if (db_ref != NULL)
	{
		DmReleaseRecord(db_ref, record_index, true);
	}
	else
	{
		MemHandleFree((*record_handle));
	}
	(*record_handle) = NULL;
}
//
//根据情况，关闭数据库
static void DmCloseDatabaseFromCardAndRAM(DmOpenRef db_ref, FileRef db_file_ref)
{
	if (db_ref != NULL)
	{
		DmCloseDatabase(db_ref);
	}
	else
	{
		VFSFileClose(db_file_ref);
	}
}
#pragma mark -
//--------------------------------------------------------------------------
//判断是否为GBK单字
static Boolean IsGBK(const Char *content, UInt16 content_length)
{
	//GB2312(A1-F7)(A1-FE)
	//Empty(F8---FE)(A1-FE) GBK5,4,3(A1---A7 A8-A9-FE)(40-A0),(81-A0)(40-FE)
	if (content_length==2)
	{
		UInt8 a=content[0], b=content[1];
		if ((a>=0x81 && a<=0xA0 && b>=0x40) || (a>=0xA1 && b>=0x40 && b<=0xA0))
			return true;
	}	
	return false;
}

//--------------------------------------------------------------------------
//获取结果中键值和内容的长度
static UInt16 GetLengthOfResultKey(Boolean filterGB, Boolean filterChar, Char *result, UInt16 *key_length, UInt16 *content_length)
{
	UInt16		key_count = 1;
	Char *content;
	
	(*key_length) = 0;
	(*content_length) = 0;
	//键值长度
	while ((UInt8)(*result) <= 0x7F && (UInt8)(*result) >0x20)
	{
		if (*result == '\'')
		{
			key_count ++;
		}
		result ++;
		(*key_length) ++;
	}
	//内容长度
	content = result;
	while ((UInt8)(*result) > 0x02) //0x00－全内容结束；0x01－本内容段结束；0x02－本内容段结束（固顶内容）
	{
		result ++;
		(*content_length) ++;
	}
	if(filterChar && ((*content_length)>=4))	//是否为词组
		return 0;
	if(filterGB && IsGBK( content, (*content_length)))//是否仅显示GB2312字符
		return 0;
	return key_count;
}
//-----------------------------------
//调用子程序
/*static void SubLaunch(const Char *nameP)
{
	  LocalID  dbID = DmFindDatabase(0, nameP);
	  if (dbID)
	    SysAppLaunch(0, dbID, 0, 60000, NULL, NULL);//60000,50011,50012,50013
}*/

//--------------------------------------------------------------------------
//将半角符号转换为全角
static void TreoKBFullwidth(Char *str)
{
	if(StrLen(str)==1 && (UInt8)str[0]<0x7F)
	{
		MemHandle  rscHandle;
		Char       *rsc;	
		rscHandle = DmGetResource(strRsc, StrFullwidth) ;
		rsc = MemHandleLock(rscHandle) ;		
		StrNCopy(str, rsc+2*((UInt8)str[0] - ' '), 2); //从字符串中读取全角字符
		MemHandleUnlock(rscHandle);
		DmReleaseResource(rscHandle);
	}
}
//
//动态符号 目前仅支持输出当前日期时间
static void TreoKBDynamicPunc(Char *str)
{
	switch (str[0])
	{
		case '?': //输出动态数据
		{
			switch (str[1])
			{				
				case 'd'://当前日期
				case 't'://当前时间
				{	
					DateTimeType now;
					TimSecondsToDateTime (TimGetSeconds(), &now);
					if (str[1] == 'd')
						DateToAscii(now.month, now.day, now.year, str[2]==0x00 ? dfYMDLongWithDot : str[2]-'0', str);				
					else
						TimeToAscii(now.hour, now.minute, str[2]==0x00 ? tfColon24h: str[2] - '0', str);
					break;
				}				
			}
			break;
		}
		/*case '!'://调用子程序
		{
			SubLaunch(&str[1]);
			MemSet(str, 15, 0x00);
			break;
		}*/
	}
}
//--------------------------------------------------------------------------
//五笔字型86/98版自动编码
static void GetWordCodes(UInt32 type, UInt8 len, Char *bufK, UInt8 created_word_count, stru_CreateWordResult *created_word)
{
	if(created_word_count==1)
	{
		StrCopy(bufK, created_word[0].index);
		return;
	}
	if(type>>16=='WB' || type>>16=='wb')
	{
		UInt8 i;
		Char code[51]="aabnnydhdefbgghhdhjgkhlgmgnayeyptrrqsvtkutntwwxcyg";
		if((type & 0xFFFF) == '86') //五笔86版
		{
			((UInt16 *)code)[8]='gi';//不
			((UInt16 *)code)[14]='yl';//为		
		}
		for(i=0; i<created_word_count; i++)
		{
			if(created_word[i].index[1]==chrNull)
				StrNCopy(created_word[i].index, code + (created_word[i].index[0]-'a')*2, 2);				
		}
	}	
	bufK[0]=created_word[0].index[0];
	if(len==2)
	{				
		bufK[1]=created_word[0].index[1];				
		bufK[2]=created_word[1].index[0];
		bufK[3]=created_word[1].index[1];
	}
	else
	{			
		UInt8 j=created_word[0].length/2;
		UInt8 i=created_word[1].length/2;
		UInt8 k=created_word[created_word_count-1].length/2;
		bufK[3]=created_word[created_word_count-1].index[k==1?(len==3?1:0):(len==3?3:(k>3?3:2))];
		if(j==1)
		{				
			bufK[1]=created_word[1].index[0];
			bufK[2]=created_word[i==1?2:1].index[i==1?0:(i==2?2:1)];					
		}
		else
		{
			bufK[1]=created_word[0].index[j==2?2:1];
			bufK[2]=created_word[j==2?1:0].index[j==2?0:2];					
		}				
	}
}
//--------------------------------------------------------------------------
//根据键值转换字符串，取回指定内容的字符
static Char *KeyTranslate(Char key, Char *sample, UInt8 mode)
{
	if(mode == GetTranslatedKey || mode ==  GetKeyToShow)
		while (*sample != '\0')
		{
			if(mode == GetKeyToShow)//1键值 2键值转换 3键值显示
				sample ++;
			if (*sample == key)
				return (sample + 1);
			while (*sample != '\'' && *sample != '\0')
				sample ++;
			if (*sample == '\'')
				sample ++;
		}	
	return NULL;
}
//--------------------------------------------------------------------------
//获取给定的记录内容中，固顶字词后的偏移量
static UInt16 GetOffsetAfterStaticWord(Char *content, UInt16 offset)
{
	UInt16		i = 0;
	
	while ((UInt8)content[i] > 0x01)
	{
		i ++;
		if (content[i] == 0x02) //遍历过一个固顶码
		{
			i ++;
			offset += i; //修正偏移量
			content += i; //修正指针
			i = 0;
		}
	}
	
	return offset;
}
//--------------------------------------------------------------------------
//检查给定索引的记录链表节点是否存在
static Boolean MBRecordNotExist(Char *index, stru_Globe *globe, stru_MBInfo *mb_info)
{
	stru_MBRecord	*mb_record;

	if (globe->mb_record_head != NULL)
	{
		mb_record = (stru_MBRecord *)globe->mb_record_head->next;
		do
		{
			if (StrNCompare(index, mb_record->index, 2) == 0)
			{
				return false;
			}
			mb_record = (stru_MBRecord *)globe->mb_record->next;
		}while (mb_record != globe->mb_record_head);
	}
	
	return true;
}
//--------------------------------------------------------------------------
//回溯结果缓存到当页的开始位置
static void RollBackResult(stru_Globe *globe)
{
	UInt8		i;
	UInt8		mask;
	
	//页数减一
	globe->page_count --;
	//回溯当前页的最后一个结果
	globe->result = (stru_Result *)globe->result->prev;
	//循环回溯至当前页的第一个结果
	i = globe->result_status[globe->page_count];
	mask = slot5;
	while (i != slot1)
	{
		if (i & mask) //该位置存在结果
		{
			globe->result = (stru_Result *)globe->result->prev;
			i &= (~mask);
		}
		mask = (mask >> 1);
	}
	//修正翻页标志
	globe->no_next = false;
	if (globe->page_count == 0)
	{
		globe->no_prev = true;
	}
	else
	{
		globe->no_prev = false;
	}
}
//--------------------------------------------------------------------------
//计算索引号
static UInt16 GetRecordIndex(Char *index)
{
	UInt16		record_index;

	switch (index[1])
	{
		case '\0':
		{
			record_index = (UInt16)(*index) - 96;
			break;
		}
		default:
		{
			record_index = ((UInt16)(*index) - 96) * 26 + (UInt16)index[1] - 96;
			break;
		}
	}

	return record_index;
}
//--------------------------------------------------------------------------
//折半查找索引，返回结果的偏移量
static UInt16 GetContentOffsetFormIndex(Char *key, Char *index, UInt16 index_size)
{
	UInt16			index_index;
	UInt16			index_offset;
	UInt16			index_min = 0;
	UInt16			index_max;
	UInt16			content_offset = 0;
	Int16			i;
	
	index_max = (index_size >> 2) - 1; //索引的最大下标
	if (MemCmp(key, index, 2) >= 0 && MemCmp(key, (index + (index_max << 2)), 2) <= 0) //索引可能存在
	{
		while (content_offset == 0 && index_min <= index_max)
		{
			index_index = ((index_min + index_max) >> 1);
			index_offset = (index_index << 2);
			i = MemCmp(key, (index + index_offset), 2);
			if (i == 0)
			{
				MemMove(&content_offset, (index + (index_offset + 2)), 2);
			}
			else if (i < 0)
			{
				index_max = index_index - 1;
			}
			else
			{
				index_min = index_index + 1;
			}
		}
	}
	
	return content_offset;
}
//--------------------------------------------------------------------------
//构造循环链表节点中的偏移量链表
static void BuildContentOffsetChain(stru_MBRecord *mb_record, Char *key, Char *index, UInt16 index_size, stru_MBInfo *mb_info)
{
	UInt16				content_offset;
	Char				i;
	Char				j;
	Char				tmp_key[3];
	stru_ContentOffset	*offset;
	
	offset = &mb_record->offset_head;
	MemMove(tmp_key, key, 3);
	//循环检索，处理万能键并构造偏移量链表
	for(i = 'a'; i <= 'z'; i ++)
	{
		if (i != mb_info->wild_char)
		{
			if (key[1] == mb_info->wild_char) //第二个字符是万能键，循环取值
			{
				tmp_key[1] = i;
			}
			else //不是万能键，修改i='z'，使循环只进行一次就退出
			{
				i = 'z';
			}
			for (j = 'a'; j <= 'z'; j ++)
			{
				if (j != mb_info->wild_char)
				{
					if (key[0] == mb_info->wild_char) //第一个字符是万能键，循环取值
					{
						tmp_key[0] = j;
					}
					else //不是万能键，修改j='z'，使循环只进行一次就退出
					{
						j = 'z';
					}
					//获取偏移量
					content_offset = GetContentOffsetFormIndex(tmp_key, index, index_size);
					//记录并构造链表
					if (content_offset > 0)
					{
						//新建节点
						offset->next = (void *)((stru_ContentOffset *)MemPtrNew(stru_ContentOffset_length));
						offset = (stru_ContentOffset *)offset->next;
						offset->key = (*((UInt16 *)tmp_key));
						offset->offset = content_offset;
						offset->next = (void *)&mb_record->offset_tail;
					}
				}
			}
		}
	}
}
//
//音节是否完整 仅适用于全拼
static Boolean NoVowel(stru_KeyBufUnit key)
{
	if(key.length>2)
		return false;
	if(key.length==2)
		if(key.content[1]=='h')
			return true;
		else
			return false;
	switch(key.content[0])
	{
		case 'a':
		case 'e':
		//case 'i':
		case 'o':
		//case 'u':
		//case 'v':
			return false;
		default:
			return true;
	}
}
//-----------------
//长码匹配
static Boolean IsMatched(stru_KeyBufUnit key, Char *content, UInt16 key_length, stru_MBInfo *mb_info)
{
	UInt16		i;
	if (key.length>key_length)
		return false;
	for (i = 0; i < key.length; i ++)
	{
		if (key.content[i] != mb_info->wild_char && key.content[i] != content[i])
		{
			return false;
		}
	}
	if(mb_info->gradually_search)	
		return true;
	return key.length==key_length;//完全匹配
}
//--------------------------------------------------------------------------
//从记录循环链表中获取结果，并正确处理万能键、从字数从多到小以及字数缩小到两个或两个一下使的记录循环链表合并的问题
static void GetResultFromMBRecord(stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16				result_count = 0;
	UInt16				key_length;
	UInt16				content_length;
	UInt16				i;
	UInt16				j;
	UInt16				blur_key_index;
	Boolean				more_result_exist = true;
	Boolean				mb_record_not_found;
	Boolean				not_matched;
	Char				*record;
	Char				*content;
	Char				*tmp;
	Char				new_key;
	MemHandle			record_handle;
	stru_MBRecord		*mb_record_start;
	stru_MBRecord		*mb_record_filter;
	stru_ContentOffset	*content_offset_unit;
	stru_Result			*result_last;
	void				*pointer_to_delete;	
	Boolean				org_gradually_search;
	
	//保存渐进查找的设置
	org_gradually_search = mb_info->gradually_search;
	if (mb_info->type == 0 && !mb_info->gradually_search && NoVowel(globe->key_buf.key[0]))
	{ //不规则码表、渐进查找关闭，若首音节不完整，则临时打开渐进查找
		mb_info->gradually_search = true;
	}
	
	//记录当前的结果节点
	if (globe->result == NULL)
	{
		result_last = &globe->result_head;
	}
	else if (globe->result == &globe->result_tail)
	{
		result_last = (stru_Result *)globe->result_tail.prev;
	}
	else
	{
		result_last = globe->result;
	}
	//循环取结果
	while (result_count < 5 && more_result_exist)
	{
		//循环记录链表，查找有结果可取，且该结果长度与期待长度一致的节点，如果没有这样的节点，递减期待长度并继续查找
		mb_record_not_found = true;
		while (mb_record_not_found && more_result_exist)
		{
			more_result_exist = false; //没有任何节点可取的标志
			mb_record_start = globe->mb_record; //开始节点
			//从当前节点开始循环一周，查找一个合适的节点
			do
			{
				if (globe->mb_record->last_word_length == globe->current_word_length && globe->mb_record->more_result_exist)
				{ //找到合适的节点
					mb_record_not_found = false;
					more_result_exist = true;
				}
				else //该节点不合适
				{
					//后移一个节点
					globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
				}
			}while (globe->mb_record != mb_record_start && mb_record_not_found); //尚未循环一周，且尚未找到结果
			//若没有找到合适的节点，根据码表类型等调整记录链表
			if (mb_info->type == 0)
			{
				//不规则编码，若没有找到任何与期待长度一致的节点，递减期待长度，直至产生可以取到结果的节点
				while ((! more_result_exist) && globe->current_word_length > 1)
				{
					//期待长度减一
					globe->current_word_length --;
					//从开始节点进行循环
					globe->mb_record = globe->mb_record_head;
					do
					{
						//未被屏蔽的节点
						if (globe->mb_record->last_word_length != 0)
						{
							//修正节点长度
							globe->mb_record->last_word_length = globe->current_word_length;
							//修改索引
							if (globe->current_word_length < 4)
							{
								globe->mb_record->index[globe->current_word_length] = '\0';
								i = globe->current_word_length; //比较索引时的比较长度
							}
							else
							{
								i = 4;
							}
							//检查并屏蔽重复的节点
							mb_record_start = globe->mb_record;
							mb_record_filter = (stru_MBRecord *)globe->mb_record->next;
							while (mb_record_filter != mb_record_start)
							{
								if (StrNCompare(mb_record_filter->index, mb_record_start->index, 4) == 0)
								{ //重复
									mb_record_filter->last_word_length = 0;
									mb_record_filter->more_result_exist = false;
								}
								mb_record_filter = (stru_MBRecord *)mb_record_filter->next;
							}
							//若收缩至只有一个键，需要重新获取记录号
							if (globe->current_word_length == 1)
							{
								globe->mb_record->record_index = GetRecordIndex(globe->mb_record->index);
							}
							//重新构造节点的偏移量链表
							//删除旧链表
							content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
							while (content_offset_unit != &globe->mb_record->offset_tail)
							{
								pointer_to_delete = (void *)content_offset_unit;
								content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
								MemPtrFree(pointer_to_delete);
							}
							globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
							//构造新链表
							DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);
							record = (Char *)MemHandleLock(record_handle);
							BuildContentOffsetChain(globe->mb_record, (globe->mb_record->index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
							DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
							if (globe->mb_record->offset_head.next == (void *)&globe->mb_record->offset_tail) //没有找到
							{ //跳过该节点
								globe->mb_record->more_result_exist = false;
							}
							else
							{
								globe->mb_record->more_result_exist = true;
								more_result_exist = true;
							}
						}
						//移动到下一个节点
						globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
					}while (globe->mb_record != globe->mb_record_head);
				}
			}
			else if (mb_info->gradually_search && (globe->key_buf.key[0].content[0] != mb_info->wild_char && globe->key_buf.key[0].content[1] != mb_info->wild_char && globe->key_buf.key[0].content[2] != mb_info->wild_char && globe->key_buf.key[0].content[3] != mb_info->wild_char))
			{
				//规则编码，若没有找到任何与期待长度一致的节点，递增期待长度，直至长度等于4
				while ((! more_result_exist) && (globe->current_word_length < mb_info->key_length/*长码*/))
				{
					//期待长度加一
					globe->current_word_length ++;
					if (globe->current_word_length == 2) //从一码增长至二码，扩展记录链表节点，重新计算记录号，获取内容偏移量
					{
						//从开始节点进行循环
						globe->mb_record = globe->mb_record_head;
						do
						{
							if (globe->mb_record->last_word_length < 2) //未扩展的记录
							{
								//首先扩展当前记录
								//构造新索引
								globe->mb_record->index[1] = 'a';
								globe->mb_record->last_word_length ++;
								//获取新记录号
								globe->mb_record->record_index = GetRecordIndex(globe->mb_record->index);
								//重新构造节点的偏移量链表
								//删除旧链表
								content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
								while (content_offset_unit != &globe->mb_record->offset_tail)
								{
									pointer_to_delete = (void *)content_offset_unit;
									content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
									MemPtrFree(pointer_to_delete);
								}
								globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
								//检查该记录是否有内容
								DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);
								record = MemHandleLock(record_handle);
								if (*((UInt16 *)record) > 0) //有内容
								{
									//构造新链表
									BuildContentOffsetChain(globe->mb_record, (globe->mb_record->index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
									globe->mb_record->more_result_exist = true;
									more_result_exist = true;
								}
								DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
								//生成其他记录
								for (new_key = 'b'; new_key <= 'z'; new_key ++)
								{
									if (new_key != mb_info->wild_char)
									{
										//新建记录
										NewMBRecord(globe);
										StrCopy(globe->mb_record->index, ((stru_MBRecord *)globe->mb_record->prev)->index);
										globe->mb_record->index[1] = new_key;
										globe->mb_record->last_word_length = 2;
										globe->mb_record->record_index = GetRecordIndex(globe->mb_record->index);
										globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
										//检查该记录是否有内容
										DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);										
										record = MemHandleLock(record_handle);
										if (*((UInt16 *)record) > 0) //有内容
										{
											//构造新链表
											BuildContentOffsetChain(globe->mb_record, (globe->mb_record->index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
											globe->mb_record->more_result_exist = true;
											more_result_exist = true;
										}
										DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
									}
								}
							}
							globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
						}while (globe->mb_record != globe->mb_record_head);
					}
					else //增长到三码或四码，重建偏移量链表
					{
						globe->mb_record = globe->mb_record_head;
						do
						{
							
							if(globe->current_word_length>3 && mb_info->key_length>4)/*长码*/
							{
								globe->current_word_length=mb_info->key_length;
								globe->mb_record->last_word_length = mb_info->key_length;
								globe->mb_record->index[3] = mb_info->wild_char;
							}
							else
								for ( ; globe->mb_record->last_word_length < globe->current_word_length; globe->mb_record->last_word_length ++)
								{
									globe->mb_record->index[globe->mb_record->last_word_length] = mb_info->wild_char;
								}
							//重新构造节点的偏移量链表
							//删除旧链表
							content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
							while (content_offset_unit != &globe->mb_record->offset_tail)
							{
								pointer_to_delete = (void *)content_offset_unit;
								content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
								MemPtrFree(pointer_to_delete);
							}
							globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
							DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);							
							record = (Char *)MemHandleLock(record_handle);
							if (*((UInt16 *)record) > 0)
							{
								//构造新链表
								BuildContentOffsetChain(globe->mb_record, (globe->mb_record->index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
								if (globe->mb_record->offset_head.next != (void *)&globe->mb_record->offset_tail)
								{
									globe->mb_record->more_result_exist = true;
									more_result_exist = true;
								}
							}
							DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
							globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
						}while (globe->mb_record != globe->mb_record_head);
					}
				}
			}
		}
		//找到一个节点取结果
		if (mb_record_not_found == false && more_result_exist == true)
		{
			//取记录
			//record_handle = DmGetRecordFromCardAndRAM(globe, globe->mb_record->record_index);
			DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);			
			//锁定到指针
			record = (Char *)MemHandleLock(record_handle);
			//一次取出一个节点的全部可能偏移量中的结果
			content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
			globe->mb_record->more_result_exist = false; //预置标志
			//循环节点的每个偏移量
			while (content_offset_unit != &globe->mb_record->offset_tail)
			{
				//偏移至结果
				content = record + content_offset_unit->offset;
				if (mb_info->type == 0) //不规则码表
				{
					//在整个内容段中检索符合关键字的一个结果
					not_matched = true;
					while (*content != '\0' && not_matched)
					{
						//记录当前偏移量
						tmp = content;
						//获取键值和内容的长度，同时判断当前结果的编码数是否符合需要
						if (GetLengthOfResultKey(globe->settingP->filterGB, globe->settingP->filterChar, content, &key_length, &content_length) == globe->current_word_length)
						{
							//取第一组音
							i = 0;
							do
							{
								globe->cache[i] = *tmp;
								tmp ++;
								i ++;
							}while ((UInt8)(*tmp) <= 0x7F && (UInt8)(*tmp) > 0x20 && *tmp != '\'');
							//匹配编码，其中第一组编码采用模糊码
							blur_key_index = 0;
							while (not_matched && blur_key_index < 4 && globe->blur_key[blur_key_index].content[0] != '\0')
							{
								if ((StrNCompare(globe->cache, globe->blur_key[blur_key_index].content, globe->blur_key[blur_key_index].length) == 0 && (mb_info->gradually_search)) || //渐进查找
									(StrCompare(globe->cache, globe->blur_key[blur_key_index].content) == 0 && (! mb_info->gradually_search))) //标准查找
								{ //第一组编码匹配了，进行后续编码的匹配
									MemSet(globe->cache, 100, 0x00);
									not_matched = false; //预置匹配标志
									j = globe->created_key + 1; //指向第一组编码之后
									//tmp ++; //内容指针指向第一组编码之后
									while ((UInt8)(*tmp) <= 0x7F && (UInt8)(*tmp) > 0x20 && (! not_matched)) //未到达汉字，且未遇到不匹配的情况
									{
										tmp ++; //内容指针指向一组编码之后
										//构筑一组编码
										i = 0;
										while ((UInt8)(*tmp) <= 0x7F && (UInt8)(*tmp) > 0x20 && *tmp != '\'')
										{
											globe->cache[i] = *tmp;
											tmp ++;
											i ++;
										}
										//比较编码
										if ((StrNCompare(globe->cache, globe->key_buf.key[j].content, globe->key_buf.key[j].length) == 0 && (mb_info->gradually_search)) || //渐进查找
											(StrCompare(globe->cache, globe->key_buf.key[j].content) == 0 && (! mb_info->gradually_search))) //标准查找
										{ //匹配
											j ++;
											//tmp ++; //移动到下一个编码，如果已经是汉字，则本操作会导致tmp指向一个汉字的低位
										}
										else //不匹配
										{
											not_matched = true; //设置标志，使循环退出
										}
										MemSet(globe->cache, 100, 0x00);
									}
								}
								blur_key_index ++;
							}
							MemSet(globe->cache, 100, 0x00);
							if (! not_matched ) //找到了匹配的结果
							{
								NewResult(globe); //新建结果链表节点
								if (content_length == 0) //英文词表
								{
									content_length = key_length+1;
									globe->result->result = MemPtrNew(content_length  + 1); //内容
									MemSet(globe->result->result, content_length  + 1, 0x00);									
									globe->result->result[0] = chrSpace;
									StrNCopy(globe->result->result + 1, content, content_length-1 );
									globe->result->length = content_length; //长度							
								}
								else
								{	
									globe->result->result = MemPtrNew(content_length + 1); //内容
									MemSet(globe->result->result, content_length + 1, 0x00);
									StrNCopy(globe->result->result, (content + key_length), content_length);
									globe->result->length = content_length; //长度
								}
								globe->result->record_index = globe->mb_record->record_index; //所在记录
								StrCopy(globe->result->index, globe->mb_record->index); //键值
								if (*(content + key_length + content_length) == '\2') //固顶字词标志
								{
									globe->result->is_static = true;
								}
								else
								{
									globe->result->is_static = false;
								}
								globe->result->offset = content_offset_unit->offset; //偏移量
								//结果计数加一
								result_count ++;
							}
						}
						//偏移量后移一个结果，如果匹配失败，则以此偏移量继续的匹配
						content_offset_unit->offset += (key_length + content_length + 1);
						content += (key_length + content_length + 1);
						if (*content != '\0') //还有下一个结果
						{
							globe->mb_record->more_result_exist = true; //设置标志
						}
					}
				}
				else //规则码表
				{
					if (*content != '\0') //有结果
					{
						//获取键值和内容的长度
						if(GetLengthOfResultKey(globe->settingP->filterGB, globe->settingP->filterChar, content, &key_length, &content_length) && (mb_info->key_length<=4 || globe->key_buf.key[0].length<4 || IsMatched(globe->key_buf.key[0], content, key_length, mb_info))/*长码*/)
						{
							NewResult(globe); //新建结果链表节点							
							globe->result->key = MemPtrNew(key_length + 1);//当前编码
							MemSet(globe->result->key, key_length + 1, 0x00);
							StrNCopy(globe->result->key, content, key_length);
							content += key_length;
							globe->result->result = MemPtrNew(content_length + 1); //当前内容
							MemSet(globe->result->result, content_length + 1, 0x00);
							StrNCopy(globe->result->result, content, content_length);
							globe->result->length = content_length; //长度
							globe->result->record_index = globe->mb_record->record_index; //所在记录
							StrNCopy(globe->result->index, globe->mb_record->index, 2); //键值
							MemMove((globe->result->index + 2), &content_offset_unit->key, 2);
							globe->result->is_static = (*(content + content_length) == '\2'); //固顶字词标志
							globe->result->offset = content_offset_unit->offset; //偏移量
							//偏移量后移一个结果
							/*content_offset_unit->offset += (key_length + content_length + 1);
							if (*(content + content_length) != '\0') //还有下一个结果
							{
								globe->mb_record->more_result_exist = true; //设置标志
							}*/
							//结果计数加一
							result_count ++;
						}
						else
							content += key_length;
						//偏移量后移一个结果，如果匹配失败，则以此偏移量继续的匹配
						content_offset_unit->offset += (key_length + content_length + 1);
						if (*(content + content_length) != '\0') //还有下一个结果
						{
							globe->mb_record->more_result_exist = true; //设置标志
						}
					}
				}
				//后移一个键值索引
				content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
			}
			//解除锁定
			DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
			//移动到下一个节点
			globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
		}
	}
	//指向新结果集的第一个节点
	if (result_count > 0) //找到新的内容
	{
		globe->result = (stru_Result *)result_last->next;
		globe->no_next = false;
	}
	else //没有找到任何内容
	{
		globe->result = &globe->result_tail;
		globe->no_next = true;
	}
	mb_info->gradually_search = org_gradually_search;
}
//--------------------------------------------------------------------------
//构造索引
static void BuildIndex(Char *index, UInt16 key_index, UInt16 key_count, UInt16 blur_key_nums, stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16	index_index = 0;
	
	//清除旧索引
	MemSet(index, 5, 0x00);
	//构造索引
	if (mb_info->type == 0) //不规则码表，先处理模糊音
	{
		//修正计数器
		key_count += key_index;
		if (mb_info->smart_offset > 0) //存在模糊音，则索引第一码取模糊音
		{
			index[0] = globe->blur_key[blur_key_nums].content[0];
			key_index ++;
			index_index ++;
		}
		//循环构筑索引
		while (key_index < key_count)
		{
			index[index_index] = globe->key_buf.key[key_index].content[0];
			key_index ++;
			if (index_index < 3)
			{
				index_index ++;
			}
		}
	}
	else //规则码表
	{
		while (key_index < key_count)
		{
			index[index_index] = globe->key_buf.key[0].content[key_index];
			key_index ++;
			if (index_index < 3)
			{
				index_index ++;
			}
		}		
		if(key_count>3 && key_count<mb_info->key_length && mb_info->key_length>4 && index[3]!=mb_info->wild_char && mb_info->gradually_search)//长码
			index[3]='\0';
	}
}
//--------------------------------------------------------------------------
//生成第一个关键字的模糊音
static void BuildBlurKey(stru_KeyBufUnit *blur_key, stru_KeyBufUnit *org_key, stru_MBInfo *mb_info)
{
	Char		*tmp;
	UInt16		blur_length;
	UInt16		i = 0;
	
	//清除模糊音缓存
	MemSet(blur_key, 510, 0x00);
	//构造第一个键
	StrCopy(blur_key[0].content, org_key->content);
	blur_key[0].length = org_key->length;
	if (mb_info->smart_offset > 0) //存在模糊音，进行构造
	{
		//处理前模糊音
		while (mb_info->blur_tail[i].key1[0] != '\0')
		{
			blur_length = StrLen(mb_info->blur_tail[i].key1); //取模糊音长度
			//匹配并构造模糊音
			if (org_key->length >= blur_length)
			{
				tmp = blur_key[0].content + (org_key->length - blur_length);
				if (StrCompare(tmp, mb_info->blur_tail[i].key1) == 0) //匹配
				{
					//构造新键
					StrCopy(tmp, mb_info->blur_tail[i].key2);
					blur_key[0].length = StrLen(blur_key[0].content);
					StrCopy(blur_key[1].content, org_key->content);
					blur_key[1].length = org_key->length;
					//跳出
					break;
				}
				else //不匹配，尝试该模糊音的key2
				{
					blur_length = StrLen(mb_info->blur_tail[i].key2);
					if (org_key->length >= blur_length)
					{
						tmp = blur_key[0].content + (org_key->length - blur_length);
						if (StrCompare(tmp, mb_info->blur_tail[i].key2) == 0)
						{
							StrCopy(tmp, mb_info->blur_tail[i].key1);
							blur_key[0].length = StrLen(blur_key[0].content);
							StrCopy(blur_key[1].content, org_key->content);
							blur_key[1].length = org_key->length;
							break;
						}
					}
				}
			}
			else
			{
				blur_length = StrLen(mb_info->blur_tail[i].key2);
				if (org_key->length >= blur_length)
				{
					tmp = blur_key[0].content + (org_key->length - blur_length);
					if (StrCompare(tmp, mb_info->blur_tail[i].key2) == 0)
					{
						StrCopy(tmp, mb_info->blur_tail[i].key1);
						blur_key[0].length = StrLen(blur_key[0].content);
						StrCopy(blur_key[1].content, org_key->content);
						blur_key[1].length = org_key->length;
						break;
					}
				}
			}
			i ++;
		}
		//处理后模糊音
		i = 0;
		while (mb_info->blur_head[i].key1[0] != '\0')
		{
			blur_length = StrLen(mb_info->blur_head[i].key1); //取模糊音长度
			//匹配并构造模糊音
			if (StrNCompare(blur_key[0].content, mb_info->blur_head[i].key1, blur_length) == 0)
			{
				if (blur_key[1].length > 0)
				{
					StrCopy(blur_key[2].content, mb_info->blur_head[i].key2);
					StrCopy(blur_key[3].content, mb_info->blur_head[i].key2);
					StrCat(blur_key[2].content, (blur_key[0].content + blur_length));
					StrCat(blur_key[3].content, (blur_key[1].content + blur_length));
					blur_key[2].length = StrLen(blur_key[2].content);
					blur_key[3].length = StrLen(blur_key[3].content);
				}
				else
				{
					StrCopy(blur_key[0].content, mb_info->blur_head[i].key2);
					StrCat(blur_key[0].content, (org_key->content + blur_length));
					blur_key[0].length = StrLen(blur_key[0].content);
					StrCopy(blur_key[1].content, org_key->content);
					blur_key[1].length = org_key->length;
				}
				break;
			}
			else
			{
				blur_length = StrLen(mb_info->blur_head[i].key2);
				if (StrNCompare(blur_key[0].content, mb_info->blur_head[i].key2, blur_length) == 0)
				{
					if (blur_key[1].length > 0)
					{
						StrCopy(blur_key[2].content, mb_info->blur_head[i].key1);
						StrCopy(blur_key[3].content, mb_info->blur_head[i].key1);
						StrCat(blur_key[2].content, (blur_key[0].content + blur_length));
						StrCat(blur_key[3].content, (blur_key[1].content + blur_length));
						blur_key[2].length = StrLen(blur_key[2].content);
						blur_key[3].length = StrLen(blur_key[3].content);
					}
					else
					{
						StrCopy(blur_key[0].content, mb_info->blur_head[i].key1);
						StrCat(blur_key[0].content, (org_key->content + blur_length));
						blur_key[0].length = StrLen(blur_key[0].content);
						StrCopy(blur_key[1].content, org_key->content);
						blur_key[1].length = org_key->length;
					}
					break;
				}
			}
			i ++;
		}
	}
}
//--------------------------------------------------------------------------
//检索码表，正确处理模糊音和万能键，生成记录循环链表及每个节点的偏移量链表，然后获取检索结果
static void SearchMB(stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16			blur_key_nums;
	UInt16			record_index;
	Boolean			org_gradually_search;
	Boolean			gradually_researched = false;
	Boolean			second_search;
	Char			index[5];
	Char			tmp_index[5];
	Char			i;
	Char			j;
	Char			*record;
	MemHandle		record_handle;
	
	
	//保存渐进查找的设置
	org_gradually_search = mb_info->gradually_search;
	//开始查找，如果是不规则码表、渐进查找关闭、且没有任何匹配的结果，则打开渐进查找检索一次
	do
	{
		//初始化记录链表
		InitMBRecord(globe);
		//初始化结果链表
		InitResult(globe);
		if (mb_info->type == 0 && globe->created_key < 10) //不规则码表
		{
			if (globe->key_buf.key[globe->created_key].length > 0)
			{
				//构造第一个关键字的模糊音
				BuildBlurKey(globe->blur_key, &globe->key_buf.key[globe->created_key], mb_info);
				globe->current_word_length = globe->key_buf.key_index + 1 - globe->created_key;
				//循环取词长度直到长度为0，取最接近期待长度的结果
				while (globe->current_word_length > 0 && globe->mb_record_head == NULL)
				{
					//循环模糊音，检索码表
					blur_key_nums = 0;
					while (blur_key_nums < 4 && globe->blur_key[blur_key_nums].content[0] != '\0')
					{
						//构造索引
						BuildIndex(index, globe->created_key, globe->current_word_length, blur_key_nums, globe, mb_info);
						if (MBRecordNotExist(index, globe, mb_info))
						{
							//获取记录号
							record_index = GetRecordIndex(index);
							//检查该记录是否有内容
							//record_handle = DmGetRecordFromCardAndRAM(globe, record_index);
							DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, record_index, &record_handle);							
							record = (Char *)MemHandleLock(record_handle);
							if ((*(UInt16 *)record) > 0) //有内容
							{
								//生成该记录的链表节点
								NewMBRecord(globe);
								globe->mb_record->record_index = record_index; //记录号
								StrCopy(globe->mb_record->index, index); //索引
								globe->mb_record->last_word_length = globe->current_word_length; //结果长度
								globe->mb_record->more_result_exist = true;
								//取符合关键字的偏移量链表
								BuildContentOffsetChain(globe->mb_record, (index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
								if (globe->mb_record->offset_head.next == (void *)&globe->mb_record->offset_tail) //没有符合关键字要求的结果
								{
									DeleteMBRecord(globe->mb_record, globe);
								}
							}
							DmReleaseRecordFromCardAndRAM(globe->db_ref, record_index, &record_handle);
						}
						blur_key_nums ++;
					}
					globe->current_word_length --;
				}
			}
		}
		else if (globe->key_buf.key[0].length > 0) //规则码表
		{
			//码长
			globe->current_word_length = globe->key_buf.key[0].length;
			//构建索引
			BuildIndex(index, 0, globe->current_word_length, 0, globe, mb_info);
			MemMove(tmp_index, index, 5);
			//循环检索码表，如果键长大于等于二，只检索一次，如果键长为一且第一次检索没有结果，
			//把第二键设置为万能键后检索第二次
			do
			{
				//循环万能键可能的范围，检索匹配的记录
				for(i = 'a'; i <= 'z'; i ++)
				{
					if (i != mb_info->wild_char)
					{
						if (tmp_index[1] == mb_info->wild_char) //第二个字符是万能键，循环取值
						{
							index[1] = i;
						}
						else //不是万能键，修改i='z'，使循环只进行一次就退出
						{
							i = 'z';
						}
						for (j = 'a'; j <= 'z'; j ++)
						{
							if (j != mb_info->wild_char)
							{
								if (tmp_index[0] == mb_info->wild_char) //第一个字符是万能键，循环取值
								{
									index[0] = j;
								}
								else //不是万能键，修改j='z'，使循环只进行一次就退出
								{
									j = 'z';
								}
								if (MBRecordNotExist(index, globe, mb_info))
								{
									//获取记录号
									record_index = GetRecordIndex(index);
									//检查该记录是否有内容
									//record_handle = DmGetRecordFromCardAndRAM(globe, record_index);
									DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, record_index, &record_handle);									
									record = (Char *)MemHandleLock(record_handle);
									if ((*(UInt16 *)record) > 0) //有内容
									{
										//生成该记录的链表节点
										NewMBRecord(globe);
										globe->mb_record->record_index = record_index; //记录号
										StrCopy(globe->mb_record->index, index); //索引
										globe->mb_record->last_word_length = globe->current_word_length; //结果长度
										globe->mb_record->more_result_exist = true;
										//取符合关键字的偏移量链表
										BuildContentOffsetChain(globe->mb_record, (index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
									}
									DmReleaseRecordFromCardAndRAM(globe->db_ref, record_index, &record_handle);
								}
							}
						}
					}
				}
				if (globe->mb_record_head == NULL && globe->current_word_length < 2 && mb_info->gradually_search)
				{
					tmp_index[1] =  mb_info->wild_char;
					globe->current_word_length = 2; //这种情况肯定原来的长度为1
					second_search = true;
				}
				else
				{
					second_search = false;
				}
			}while (second_search);
			globe->current_word_length --;
		}
		//其他全局设置
		globe->no_prev = true;
		globe->page_count = 0;
		MemSet(globe->result_status, 100, 0x00);
		if (globe->mb_record_head != NULL)
		{
			globe->current_word_length ++;
			//获取结果
			GetResultFromMBRecord(globe, mb_info);
		}
		else
		{
			globe->no_next = true;
		}
	
		//...
		if (!mb_info->gradually_search  && (! gradually_researched) && (globe->result == &globe->result_tail || ((globe->result->length>>1) < globe->key_buf.key_index+1)/*不完全匹配*/))
		{ //智能渐进，渐进查找关闭时，无完全匹配结果时，再查找一次
			mb_info->gradually_search = true;
			gradually_researched = true;
		}
		else
			gradually_researched = false;
	}while (gradually_researched);
	//恢复渐进查找的设置
	mb_info->gradually_search = org_gradually_search;
}
//--------------------------------------------------------------------------
//检查是否只有且仅有一个结果
static Boolean HasOnlyOneResult(stru_Globe *globe)
{
	stru_Result		*result;
	
	if (globe->result_head.next != (void *)&globe->result_tail) //有结果
	{
		result = (stru_Result *)globe->result_head.next; //指向第一个结果
		if (result->next == (void *)&globe->result_tail) //第一个结果的下一个结果就是表尾
		{
			return true;
		}
	}
	
	return false;
}
#pragma mark -
//--------------------------------------------------------------------------
//检查关键字中是否包含万能键，若有返回真
static Boolean KeyHasWildChar(Char *key, UInt16 key_length, stru_MBInfo *mb_info)
{
	UInt16		i;	
	for (i = 0; i < key_length; i ++)
	{
		if (key[i] == mb_info->wild_char)
		{
			return true;
		}
	}	
	return false;
}
//--------------------------------------------------------------------------
//绘制待选字
static void DrawResult(stru_Globe *globe, stru_Pref *pref)
{
	UInt8				slot = 1;
	UInt8				move_bit = 0;
	UInt8				i = 0;
	Boolean				shouldShowFiveResult = true, is_small, is_grad;
	UInt16				lineCharsCount;
	UInt16				width;
	Int16				width_left = 0;
	Int16				width_right = 0;	
	Coord				x;
	Coord				y;
	FontID				font;
	RGBColorType		foreColor;
	RGBColorType		backColor;
	
	if (globe->page_count < 100 && globe->result_head.next != (void *)&globe->result_tail) //有结果且未达到100页
	{
		font = FntSetFont(pref->displayFont);
		is_small = (pref->displayFont == stdFont) || (pref->displayFont == boldFont);
		is_grad = (pref->curMBInfo.type == 1) && (pref->curMBInfo.gradually_search);
		globe->result_status[globe->page_count] = 0;
		while (globe->result != &globe->result_tail && i < 5 && shouldShowFiveResult)
		{
			//计算结果位置
			width = FntCharsWidth(globe->result->result, globe->result->length);
			if (globe->result->length > 8)	//超过四字，仅显示一个结果
			{
				if (i == 0)		//是第一个，可以显示
				{
					lineCharsCount = globe->result->length;
					y = 16 + (globe->resultRect[0].extent.y - globe->curCharHeight) / 2;
					x = 77 - width / 2;
					shouldShowFiveResult = false;
				}
				else	//不是，退出
				{
					break;
				}
			}
			else	//四字以内，一屏显示5个结果
			{
				if (is_small && globe->result->length < 8 && globe->result->result[0]!=chrSpace)	//标准字体或粗体
				{
					lineCharsCount = 6;	//三个汉字
				}
				else
				{
					lineCharsCount = 4;	//两个汉字
				}
				if (globe->result->length <= lineCharsCount )
				{
					y = 16 + (globe->resultRect[0].extent.y - globe->curCharHeight) / 2;
					x = globe->resultRect[i].topLeft.x + 15 - width / 2;
					lineCharsCount = globe->result->length;
				}
				else	//需要换行显示
				{
					y = 16 + globe->resultRect[0].extent.y / 2 - globe->curCharHeight;
					x = globe->resultRect[i].topLeft.x + 15 - FntCharsWidth(globe->result->result, 4) / 2;
				}
			}
			
			//显示逐码提示
			if (is_grad)
			{
				Char *tmp=NULL;
				if(KeyHasWildChar(globe->key_buf.key[0].content, globe->key_buf.key[0].length, &pref->curMBInfo))
					tmp = globe->result->key;
				else if(globe->key_buf.key[0].length < pref->curMBInfo.key_length)
					tmp = (globe->result->key + globe->key_buf.key[0].length);		
				if(tmp)
				{
					FntSetFont(stdFont);
					WinDrawChars(tmp, StrLen(tmp), x, globe->imeFormRectangle.extent.y - 10);
					FntSetFont(pref->displayFont);
				}
			}
			
			//显示一个结果
			if (move_bit == globe->cursor)	//处于光标位置，高亮显示
			{
				WinSetTextColorRGB(&pref->resultHighlightForeColor, &foreColor);
				WinSetBackColorRGB(&pref->resultHighlightBackColor, &backColor);
			}
			else	//低亮度底
			{
				WinSetTextColorRGB(&pref->resultForeColor, &foreColor);
				WinSetBackColorRGB(&pref->resultBackColor, &backColor);
			}
			
			if (globe->result->length <= 8)
			{
				WinEraseRectangle(&globe->resultRect[i], 3);
			}
			else
			{
				globe->oneResultRect.topLeft.x = x - 2;
				globe->oneResultRect.extent.x = width + 4;
				WinEraseRectangle(&globe->oneResultRect, 3);
			}
			
			if (globe->result->result[0]!=chrSpace)
			{
				if (globe->result->length > lineCharsCount)	//需要换行显示
				{
					WinDrawChars(globe->result->result, lineCharsCount, x, y);
					y += globe->curCharHeight;
					WinDrawChars((globe->result->result + lineCharsCount), globe->result->length - lineCharsCount, x, y);
				}
				else
				{
					WinDrawChars(globe->result->result, globe->result->length, x, y);
				}
			}
			else//英文码表
			{
				if (globe->result->length > lineCharsCount + 1)	//需要换行显示
				{
					WinDrawChars(globe->result->result + 1, lineCharsCount, x, y);
					y += globe->curCharHeight;
					WinDrawChars((globe->result->result + 1 + lineCharsCount), globe->result->length - 1 - lineCharsCount, x, y);
				}
				else
				{
					WinDrawChars(globe->result->result + 1, globe->result->length - 1, x, y);
				}				
			}
			
			WinSetTextColorRGB(&foreColor, NULL);
			WinSetBackColorRGB(&backColor, NULL);
			
			//显示固顶标记
			if (globe->result->is_static)
			{
				y += globe->curCharHeight;
				WinDrawLine(x, y, x + globe->curCharWidth * (lineCharsCount / 2) - 1, y);
			}
			//记录该位置
			globe->result_status[globe->page_count] |= (slot << move_bit);
			move_bit ++;
			i ++;
			
			//取下一个结果
			globe->result = (stru_Result *)globe->result->next; //移动到下一个
			if (globe->result == &globe->result_tail) //到达结果的结尾，取新的结果
			{
				GetResultFromMBRecord(globe, &pref->curMBInfo);
			}
		}
		//页码总数
		globe->page_count ++;
		//下页标志
		globe->no_next = (globe->result == &globe->result_tail); //没有下一个结果了
		FntSetFont(font);
	}
}
//--------------------------------------------------------------------------
//绘制关键字
static void DrawKey(stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16	i = 0;
	UInt16	j = 0;
	Int16	k;
	UInt16	key_length;
	Char	*tmp;
	FontID	font;
	
	if (globe->in_create_word_mode) //自造词模式
	{
		//构造已完成的自造词
		for (i = 0; i < globe->created_word_count; i ++)
		{
			StrCat(globe->cache, globe->created_word[i].result);
		}
		i = globe->created_key;
	}
	if (globe->english_mode)
	{
		//英文模式，直接把关键字列印出来
		if (i <= globe->key_buf.key_index)
		{
			for ( ; i <= globe->key_buf.key_index; i ++)
			{
				StrCat(globe->cache, globe->key_buf.key[i].content);
			}
		}
		key_length = StrLen(globe->cache);
	}
	else
	{
		//构造关键字串，如果是普通模式，则从第一个关键字开始，否则从未完成造词的关键字开始
		if (i <= globe->key_buf.key_index)
		{
			if (mb_info->translate_offset > 0) //需进行键值转换
			{
				j = StrLen(globe->cache);
				for (k = 0; k < globe->key_buf.key[i].length; k ++)
				{
					tmp = KeyTranslate(globe->key_buf.key[i].content[k], mb_info->key_translate, GetKeyToShow);
					if (tmp != NULL)
					{
						while (*tmp != '\'' && *tmp != '\0')
						{
							globe->cache[j] = *tmp;
							j ++;
							tmp ++;
						}
					}
				}
				i ++;
				for ( ; i <= globe->key_buf.key_index; i ++)
				{
					StrCat(globe->cache, "\'");
					j ++;
					for (k = 0; k < globe->key_buf.key[i].length; k ++)
					{
						tmp = KeyTranslate(globe->key_buf.key[i].content[k], mb_info->key_translate, GetKeyToShow);
						if (tmp != NULL)
						{
							while (*tmp != '\'' && *tmp != '\0')
							{
								globe->cache[j] = *tmp;
								j ++;
								tmp ++;
							}
						}
					}
				}
			}
			else //不需要进行键值转换
			{
				StrCat(globe->cache, globe->key_buf.key[i].content);
				i ++;
				for ( ; i <= globe->key_buf.key_index; i ++)
				{
					StrCat(globe->cache, "\'");
					StrCat(globe->cache, globe->key_buf.key[i].content);
				}
			}
		}
		//长度
		key_length = StrLen(globe->cache);
		//是否显示“'”
		if (globe->new_key)
		{
			StrCat(globe->cache, "\'");
			key_length ++;
		}
	}
	//设置字体
	font = FntSetFont(stdFont);
	//画关键字串
	k = 77 - FntCharsWidth(globe->cache, key_length) / 2;
	if (k < 35)
	{
		k = 35;
	}
	WinDrawTruncChars(globe->cache, key_length, k, 2, 132 - k);
	//恢复字体
	FntSetFont(font);
	//清空缓存
	MemSet(globe->cache, key_length, 0x00);
}
//--------------------------------------------------------------------------
static Boolean isGrfLocked(stru_Pref *pref)
{
	Boolean	capsLock = false;
	Boolean	numLock = false;
	Boolean	optLock = false;
	Boolean	autoShifted = false;
	UInt16	tempShift = 0;
	
	if (pref->isTreo)
	{
		HsGrfGetStateExt(&capsLock, &numLock, &optLock, &tempShift,&autoShifted);
	}
	else
	{
		GrfGetState(&capsLock, &numLock, &tempShift, &autoShifted);
	}
	
	if (tempShift == grfTempShiftUpper || tempShift == hsGrfTempShiftOpt)
	{
		optLock = true;
	}
	
	return (capsLock | numLock | optLock);
}

//绘制输入框
static void DrawIMEForm(FormType *form, RectangleType *form_rect, stru_Globe *globe, stru_Pref *pref, UInt16 type)
{
	FontID				font_id;
	RGBColorType		preventTextColor;
	RGBColorType		preventBackColor;
	RectangleType		rectangle;
	WinHandle			current_window;
	WinHandle			gsi_save;
	UInt16				error;
	UInt16				strID=0;
	
	//在绘图缓存中绘图
	current_window = WinSetDrawWindow(globe->draw_buf);
	WinEraseWindow();
	
	//显示内容
	DrawResult(globe, pref);
	
	//画关键字区域
	WinSetTextColorRGB(&pref->codeForeColor, &preventTextColor);
	WinSetBackColorRGB(&pref->codeBackColor, &preventBackColor);
	rectangle.topLeft.x = 1;
	rectangle.topLeft.y = 1;
	rectangle.extent.x = 152;
	rectangle.extent.y = 14;
	WinEraseRectangle(&rectangle, 0);
	DrawKey(globe, &pref->curMBInfo);
	if (globe->in_create_word_mode && !pref->menu_button) //自造词
	{
		strID = StrWord;
	}
	else if (globe->english_mode && !pref->menu_button) //英文模式
	{
		strID = StrEng;
	}
	else if(pref->activeStatus & tempMBSwitchMask)//临时码表模式
	{
		strID = StrTemp;
	}
	if(strID)
	{
		MemHandle  rscHandle;
		Char       *rsc;	
		rscHandle = DmGetResource(strRsc, strID) ;
		rsc = MemHandleLock(rscHandle) ;		
		WinDrawChars(rsc, StrLen(rsc), 2, 2);
		MemHandleUnlock(rscHandle);
		DmReleaseResource(rscHandle);	
	}
	else if(pref->menu_button)//码表名称
	{
		CtlSetLabel (FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrMENU)),pref->curMBInfo.name);
		//CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrMENU)));
	}
	else //码表名称
	{
		WinDrawChars(pref->curMBInfo.name, StrLen(pref->curMBInfo.name), 2, 2);
	}
	
	//上下翻页标志
	/*if (!pref->choice_button)//翻页按钮
	{
	font_id = FntSetFont(symbol7Font); //设置字体
	WinDrawChar((globe->no_prev?0x0003:0x0001), 144, 0); //灰或黑色上箭头
	WinDrawChar((globe->no_next?0x0004:0x0002), 144, 7); //灰或黑色下箭头
	FntSetFont(font_id);
	}*/
	
	//恢复颜色
	WinSetTextColorRGB(&preventTextColor, NULL);
	WinSetBackColorRGB(&preventBackColor, NULL);
	
	//拷贝绘图缓存
	WinSetDrawWindow(current_window); //恢复绘图窗口
	
	if (isGrfLocked(pref))
	{
		rectangle.topLeft.x = 132;
		rectangle.topLeft.y = 2;
		rectangle.extent.x = 10;
		rectangle.extent.y = 10;
		gsi_save = WinSaveBits(&rectangle, &error); //保存gsi指示器
		WinCopyRectangle(globe->draw_buf, current_window, form_rect, 1, 1, winPaint); //把缓存拷贝至窗口
		WinRestoreBits(gsi_save, 132, 2); //恢复gsi指示器
	}
	else
	{	
		WinCopyRectangle(globe->draw_buf, current_window, form_rect, 1, 1, winPaint); //把缓存拷贝至窗口
	}
	
	//WinCopyRectangle(globe->draw_buf, current_window, form_rect, 1, 1, winPaint); //把缓存拷贝至窗口
	//恢复输入框按钮
	
	if (pref->choice_button)//翻页按钮
	{
		CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrUP)));
		CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrDOWN)));
	}
	else//上下翻页标志
	{
		font_id = FntSetFont(symbol7Font); //设置字体
		WinDrawChar((globe->no_prev?0x0003:0x0001), 144, 0); //灰或黑色上箭头
		WinDrawChar((globe->no_next?0x0004:0x0002), 144, 7); //灰或黑色下箭头
		FntSetFont(font_id);
	}
	/*if (pref->menu_button)//菜单按钮
	{
		CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrMENU)));
	}*/
	//if (StrCaselessCompare(pref->curMBInfo.file_name + (StrLen(pref->curMBInfo.file_name) - 3), "GBK")==0)//字符集
	//	CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrGBK)));
	//窗体边框
	//WinSetForeColorRGB(&pref->frameColor, &preventTextColor);
	//WinDrawRectangleFrame(popupFrame, form_rect);
	WinSetForeColorRGB(&preventTextColor, NULL);
}
#pragma mark -

//--------------------------------------------------------------------------
//调整选择结果的词频位置
static void FixWordOffset(stru_Result *result, stru_Globe *globe, UInt8 mode, Boolean set_static)
{
	UInt16		n;
	UInt16		x;
	UInt16		step;
	UInt16		offset;
	UInt16		i;
	Char		*tmp;
	Char		*record;
	MemHandle	record_handle;
	
	//获取要调整词频的结果所在的记录，以及结果所在的记录段的偏移量
	record_handle = DmGetRecord(globe->db_ref, result->record_index); //获取记录
	record = (Char *)MemHandleLock(record_handle);
	offset = GetContentOffsetFormIndex((result->index + 2), (record + (*((UInt16 *)record))), MemHandleSize(record_handle) - (*((UInt16 *)record)));
	//跳过固顶字词，获取固顶字词后的第一个内容的偏移量
	offset = GetOffsetAfterStaticWord((record + offset), offset);
	if (offset < result->offset) //需要进行词频调整
	{
		n = StrLen((record + offset)); //段长度
		//计算往前进的步长
		if (mode == fixModeNormal) //正常模式
		{
			step = ((result->offset - offset) >> 1);
			if (step == 0)
			{
				step = 1;
			}
			//往前微调步长，直至找到一个完整的内容单元
			while ((result->offset - step >= offset) && (*(record + (result->offset - step)) != '\1' || step == 1))
			{
				step ++;
			}
			step --;
		}
		else //强制移动到第一位
		{
			step = result->offset - offset;
		}
		//读取要调整词频的结果的完整内容单元
		tmp = (record + result->offset) - 1;
		i = 0;
		do
		{
			tmp ++;
			globe->cache[i] = *tmp;
			i ++;
		}while ((UInt8)(*tmp) > 0x02);
		if (set_static)
		{
			globe->cache[i - 1] = '\2';
		}
		//得到插入要调整词频的结果所在的偏移量、从该偏移量至要调整词频的结果之前的段长度
		x = result->offset - step; //插入的位置
		n = result->offset - x; //后移的内容的长度
		//从插入偏移量后移记录内容
		DmWrite(record, x + StrLen(globe->cache), (record + x), n);
		//写入要调整词频的结果
		DmWrite(record, x, globe->cache, StrLen(globe->cache));
	}
	else if (offset == result->offset && set_static) //把第一个结果设置为固顶
	{
		while ((UInt8)record[offset] > 0x02)
		{
			offset ++;
		}
		DmSet(record, offset, 1, 0x02);
	}
	//释放记录
	MemHandleUnlock(record_handle);
	DmReleaseRecord(globe->db_ref, result->record_index, true);
}

//--------------------------------------------------------------------------
//强制提前选定的词组，操作后，若需要重新查找码表，返回真，否则返回假
static Boolean MoveWordToTop(stru_Globe *globe, Boolean set_static)
{
	UInt16			i;
	UInt16			selector;
	stru_Result		*result_prev;
	
	if (globe->page_count > 0) //有结果
	{
		//记录当前结果
		result_prev = globe->result;
		//回溯当前页的开头
		RollBackResult(globe);
		selector = (slot1 << globe->cursor);
		if ((globe->result_status[globe->page_count] & selector)) //光标选择的结果存在
		{
			//移动到用户选择的结果
			i = 1;
			while (i != selector)
			{
				globe->result = (stru_Result *)globe->result->next;
				i = (i << 1);
			}
			//强制提前结果
			FixWordOffset(globe->result, globe, fixModeTop, set_static);
			return true;
		}
		else //恢复结果记录状态
		{
			globe->result = result_prev;
		}
	}
	return false;
}
//--------------------------------------------------------------------------
//解除字词的固顶码
static void UnsetStaticWord(stru_Globe *globe)
{
	UInt16			i;
	UInt16			selector;
	UInt16			content_size = 0;
	UInt16			offset;
	UInt16			move_up_size;
	Char			*record;
	MemHandle		record_handle;
	
	if (globe->page_count > 0) //有结果
	{
		//回溯当前页的开头
		RollBackResult(globe);
		selector = (slot1 << globe->cursor);
		if ((globe->result_status[globe->page_count] & selector)) //光标选择的结果存在
		{
			//移动到用户选择的结果
			i = 1;
			while (i != selector)
			{
				globe->result = (stru_Result *)globe->result->next;
				i = (i << 1);
			}
			if (globe->result->is_static) //是固顶字词
			{
				//获取记录
				record_handle = DmGetRecord(globe->db_ref, globe->result->record_index);
				record = (Char *)MemHandleLock(record_handle);
				//偏移到当前固顶结果
				offset = globe->result->offset;
				//读取完整的结果内容
				while ((UInt8)(record[offset]) > 0x02)
				{
					globe->cache[content_size] = record[offset];
					offset ++;
					content_size ++;
				}
				globe->cache[content_size] = '\1'; //取消固顶标记
				content_size ++;
				offset ++; //指向其后的内容
				//取非固顶字词的第一个结果的偏移量，减去当前固顶结果后的第一个结果的偏移量，得出要前移的内容的长度
				move_up_size = GetOffsetAfterStaticWord((record + offset), offset) - offset;
				//前移内容
				if (move_up_size > 0)
				{
					DmWrite(record, globe->result->offset, (record + offset), move_up_size);
					offset = globe->result->offset + move_up_size;
				}
				else
				{
					offset = globe->result->offset;
				}
				//写入当前固顶内容（已取消固顶标记）
				DmWrite(record, offset, globe->cache, content_size);
				//清空缓存
				MemSet(globe->cache, content_size, 0x00);
				//释放记录
				MemHandleUnlock(record_handle);
				DmReleaseRecord(globe->db_ref, globe->result->record_index, true);
			}
		}
	}
}
//--------------------------------------------------------------------------
//删除选定的词组，操作后，若需要重新查找码表，返回真，否则返回假
static Boolean DeleteWord(stru_Globe *globe)
{
	UInt16			i;
	UInt16			selector;
	UInt16			index_offset;
	UInt16			content_size = 0;
	UInt16			index_size;
	UInt16			record_size;
	Char			*tmp;
	Char			*record;
	MemHandle		record_handle;
	stru_Result		*result_prev;
	
	if (globe->page_count > 0) //有结果
	{
		//记录当前结果
		result_prev = globe->result;
		//回溯当前页的开头
		RollBackResult(globe);
		selector = (slot1 << globe->cursor);
		if ((globe->result_status[globe->page_count] & selector)) //光标选择的结果存在
		{
			//移动到用户选择的结果
			i = 1;
			while (i != selector)
			{
				globe->result = (stru_Result *)globe->result->next;
				i = (i << 1);
			}
			if (globe->result->length > 2) //不是单字
			{
				//获取记录
				record_handle = DmGetRecord(globe->db_ref, globe->result->record_index);
				record_size = MemHandleSize(record_handle);
				record = (Char *)MemHandleLock(record_handle);
				//获取索引偏移量
				index_offset = (*((UInt16 *)record));
				//获取索引段的长度
				index_size = record_size - index_offset;
				//获取要删除的结果的内容段的长度
				tmp = record + globe->result->offset;
				while ((UInt8)(*tmp) > 0x02)
				{
					content_size ++;
					tmp ++;
				}
				content_size ++;
				//查找被删除的结果所对应的索引（该索引肯定存在，所以不需要设置边界判断）
				tmp = record + index_offset;
				i = 0;
				while (MemCmp(tmp, (globe->result->index + 2), 2) != 0)
				{
					tmp += 4;
					i += 4;
				}
				//修正之后的索引的偏移量
				tmp += 4;
				i += 4;
				while (i < index_size)
				{
					MemMove(&index_offset, (tmp + 2), 2); //获取偏移量
					index_offset -= content_size; //修正
					DmWrite(record, ((*((UInt16 *)record)) + i + 2), &index_offset, 2); //写入新值
					tmp += 4;
					i += 4;
				}
				//前移记录内容，覆盖要删除的记录
				DmWrite(record, globe->result->offset, (record + (globe->result->offset + content_size)), record_size - globe->result->offset - content_size);
				//修改索引偏移量
				index_offset = (*((UInt16 *)record)) - content_size;
				DmWrite(record, 0, &index_offset, 2);
				//释放记录
				MemHandleUnlock(record_handle);
				DmReleaseRecord(globe->db_ref, globe->result->record_index, true);
				//收缩记录
				DmResizeRecord(globe->db_ref, globe->result->record_index, record_size - content_size);
			}
			return true;
		}
		else //恢复结果记录状态
		{
			globe->result = result_prev;
		}
	}
	
	return false;
}
//--------------------------------------------------------------------------
//保存自造词至码表
static void SaveWord(Char *index, Char *content, stru_Globe *globe, stru_MBInfo *mb_info)
{
	Int16				memcmp_result;
	UInt16				i;
	UInt16				j;
	UInt16				content_count;
	UInt16				new_word_unit_length = 0;
	UInt16				record_index;
	UInt16				record_size;
	UInt16				index_offset;
	UInt16				index_size;
	UInt16				read_index_size;
	UInt16				current_index_size;
	UInt16				content_offset;
	Char				*key_index;
	Char				*tmp_index;
	Char				*tmp_content;
	Char				*record;
	MemHandle			record_handle;
	stru_KeyBuf			*key_buf = NULL;
	
	//释放内存
	InitResult(globe);
	InitMBRecord(globe);
					
	//---------------------------构造新单元--------------------------
	if (mb_info->type == 0) //不规则码表，需先构造关键字（同时构造完整的关键字串），再构造索引
	{
		//创建缓存
		key_buf = (stru_KeyBuf *)MemPtrNew(sizeof(stru_KeyBuf));
		MemSet(key_buf, sizeof(stru_KeyBuf), 0x00);
		//循环构造键值
		for (i = 0; i < globe->created_word_count; i ++)
		{
			//获取字数
			content_count = (globe->created_word[i].length >> 1);
			//取记录并偏移至缓存中保存的结果的位置
			record_handle = DmQueryRecord(globe->db_ref, globe->created_word[i].record_index);
			record = (((Char *)MemHandleLock(record_handle)) + globe->created_word[i].offset);
			//循环获取当前自造词缓存的键值
			for (j = 0; j < content_count; j ++)
			{
				//获取键值
				while (*record != '\'' && ((UInt8)(*record)) < 0x80)
				{
					//键值
					key_buf->key[key_buf->key_index].content[key_buf->key[key_buf->key_index].length] = (*record);
					key_buf->key[key_buf->key_index].length ++;
					//关键字串
					globe->cache[new_word_unit_length] = (*record);
					new_word_unit_length ++;
					record ++;
				}
				//添加隔音符号“'”
				globe->cache[new_word_unit_length] = '\'';
				record ++;
				new_word_unit_length ++;
				key_buf->key_index ++;
			}
			//释放记录
			MemHandleUnlock(record_handle);
		}
		key_buf->key_index --;
		//关键字串最后冗余一个“'”，消除它
		new_word_unit_length --;
		globe->cache[new_word_unit_length] = '\0';
		//循环构造索引
		j = 0;
		for (i = 0; i <= key_buf->key_index; i ++)
		{
			index[j] = key_buf->key[i].content[0];
			if (j < 3)
			{
				j ++;
			}
		}
		//构造整个新词组
		StrCat(globe->cache, content);
		StrCat(globe->cache, "\1");
		//获取新词组的总长度（不包含结尾的0x00）
		new_word_unit_length += StrLen(content) + 1;
	}
	else //规则码表，直接构造索引
	{
		//构造整个新词组
		StrCopy(globe->cache, content);
		StrCat(globe->cache, "\1");
		//获取新词组的总长度（不包含结尾的0x00）
		new_word_unit_length = StrLen(globe->cache);
	}
	key_index = (index + 2);
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//  本段结果：
	//  globe->cache			- 要插入的完整内容段，包括编码、文字和结尾标识符0x01
	//  new_word_unit_length	- 完整内容段的长度，此长度不包括字符串结尾的0x00
	//  key_index				- 新词组的记录内索引值
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//---------------------------调整记录内容--------------------------
	//获取新词组的记录号
	record_index = GetRecordIndex(index);
	//取记录、记录长度
	record_handle = DmGetRecord(globe->db_ref, record_index);
	record_size = MemHandleSize(record_handle);
	record = (Char *)MemHandleLock(record_handle);
	//获取索引偏移量
	MemMove(&index_offset, record, 2);
	if (index_offset == 0) //如果这是一个没有任何内容的记录，把记录扩展成拥有一个索引的空记录，索引值为当前要插入的词的索引
	{
		MemHandleUnlock(record_handle); //解锁
		DmReleaseRecord(globe->db_ref, record_index, false);
		DmResizeRecord(globe->db_ref, record_index, 7); //调整尺寸
		record_handle = DmGetRecord(globe->db_ref, record_index);
		record = (Char *)MemHandleLock(record_handle);
		DmSet(record, 0, 7, 0x00); //清空
		DmSet(record, 1, 1, 0x03); //索引偏移量
		DmWrite(record, 3, key_index, 2); //索引值
		DmSet(record, 6, 1, 0x02); //内容偏移量
		index_offset = 3;
		record_size = 7;
	}
	else if (MemCmp((record + (record_size - 4)), key_index, 2) < 0) //记录中最大的索引都小于新建的词组的索引
	{
		MemHandleUnlock(record_handle);
		DmReleaseRecord(globe->db_ref, record_index, false);
		DmResizeRecord(globe->db_ref, record_index, record_size + 5);
		record_handle = DmGetRecord(globe->db_ref, record_index);
		record = (Char *)MemHandleLock(record_handle);
		DmWrite(record, index_offset + 1, (record + index_offset), record_size - index_offset);
		DmSet(record, index_offset, 1, 0x00);
		DmWrite(record, record_size + 1, key_index, 2);
		DmWrite(record, record_size + 3, &index_offset, 2);
		index_offset ++;
		record_size += 5;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//  本段结果：
	//  index_offset			- 记录索引段的偏移量
	//  record					- 经处理的记录段，保证不是一个“空”的记录
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//---------------------------写入记录------------------------------
	//遍历索引链表，检索匹配的节点，或插入新节点，并正确调整索引节点的内容偏移量、整个记录的尺寸以及索引的偏移量
	index_size = record_size - index_offset; //索引段长度
	tmp_index = (record + index_offset); //获取索引段
	for (read_index_size = 0; read_index_size < index_size; read_index_size += 4)
	{
		memcmp_result = MemCmp(tmp_index, key_index, 2);
		if (memcmp_result == 0) //找到匹配的节点
		{
			//匹配节点，内容增长对应的长度，修正节点之后的内容偏移量、索引偏移量，记录尺寸的增长不包括节点尺寸
			//获取记录偏移量
			MemMove(&content_offset, (tmp_index + 2), 2);
			//修正节点之后的内容偏移量
			tmp_index += 4;
			read_index_size += 4;
			for (; read_index_size < index_size; read_index_size += 4)
			{
				//获取该节点的内容偏移量
				MemMove(&i, (tmp_index + 2), 2);
				//修正偏移量
				i += new_word_unit_length;
				//写去偏移量
				DmWrite(record, index_offset + read_index_size + 2, &i, 2);
				//移动到下一个索引节点
				tmp_index += 4;
			}
			//增长记录长度
			MemHandleUnlock(record_handle);
			DmReleaseRecord(globe->db_ref, record_index, true);
			DmResizeRecord(globe->db_ref, record_index, (record_size + new_word_unit_length));
			record_handle = DmGetRecord(globe->db_ref, record_index);
			record = (Char *)MemHandleLock(record_handle);
			//偏移至内容段，并跳过固顶码
			content_offset += GetOffsetAfterStaticWord((record + content_offset), 0); //固顶码
			tmp_content = (record + content_offset); //获取内容段
			//后移内容
			DmWrite(record, (content_offset + new_word_unit_length), tmp_content, (record_size - content_offset));
			//填写新内容
			DmWrite(record, content_offset, globe->cache, new_word_unit_length);
			//修正记录尺寸
			record_size += new_word_unit_length;
			//修正索引偏移量
			index_offset += new_word_unit_length;
			DmWrite(record, 0, &index_offset, 2);
			//跳出循环
			break;
		}
		else if (memcmp_result > 0) //匹配的节点不存在
		{
			//新增一个空节点，修正新节点之后的内容偏移量、索引偏移量，记录尺寸的增长包括节点尺寸
			//将在当前节点的位置插入新索引节点，先对当前节点及其后的节点的索引进行修正
			MemMove(&content_offset, (tmp_index + 2), 2); //获取当前索引所指的内容偏移量
			//记录当前节点的偏移量
			j = index_offset + read_index_size;
			current_index_size = read_index_size;
			//循环修正当前索引及其后的索引的内容偏移量
			for ( ; read_index_size < index_size; read_index_size += 4)
			{
				MemMove(&i, (tmp_index + 2), 2); //读取
				i ++; //空节点长度为1
				DmWrite(record, index_offset + read_index_size + 2, &i, 2); //写入
				tmp_index += 4;
			}
			//增长记录长度
			MemHandleUnlock(record_handle);
			DmReleaseRecord(globe->db_ref, record_index, true);
			DmResizeRecord(globe->db_ref, record_index, (record_size + 5)); //空索引4，空内容1
			record_handle = DmGetRecord(globe->db_ref, record_index);
			record = (Char *)MemHandleLock(record_handle);
			//从内容偏移量位置后移1个字节，产生空内容节点
			DmWrite(record, content_offset + 1, (record + content_offset), record_size - content_offset);
			DmSet(record, content_offset, 1, 0x00);
			//从当前索引节点位置后移4个字节，产生空索引节点
			j ++; //空内容节点导致索引偏移量增加了1
			DmWrite(record, j + 4, (record + j), record_size - j + 1);
			//在空索引节点填入空内容节点的偏移量
			DmWrite(record, j, key_index, 2); //键值
			DmWrite(record, j + 2, &content_offset, 2); //偏移量
			//修正记录尺寸
			record_size += 5;
			//修正索引偏移量
			index_offset ++;
			DmWrite(record, 0, &index_offset, 2);
			//修正当前索引指针
			tmp_index = record + (j - 4); //指向上一个节点，以便在下一次循环时指向本节点
			read_index_size = current_index_size - 4;
			index_size += 4; //索引总数加1
		}
		tmp_index += 4;
	}
	
	//释放记录
	MemHandleUnlock(record_handle);
	DmReleaseRecord(globe->db_ref, record_index, true);
	//释放内存
	if (key_buf != NULL)
	{
		MemPtrFree(key_buf);
	}
}

#pragma mark -

//--------------------------------------------------------------------------
//返回一个最接近用户选择的结果的位置
static Boolean GetNearlySelector(UInt8 *selector, stru_Globe *globe, UInt16 page)
{
	if ((globe->result_status[page] & (*selector))) //直接对应
	{
		return true;
	}
	else if ((*selector) == slot5) //slot5没有结果的话，尝试slot3
	{
		if ((globe->result_status[page] & slot3)) //有结果
		{
			(*selector) = slot3;
			return true;
		}
	}
	else if ((*selector) == slot4) //slot4没有结果的话，尝试slot2
	{
		if ((globe->result_status[page] & slot2)) //有结果
		{
			(*selector) = slot2;
			return true;
		}
	}
	return false;
}
//--------------------------------------------------------------------------
//返回选定的结果，并正确处理自造词的情形
static Boolean SelectResult(Char *buf, UInt8 *operation, UInt8 selector, stru_Globe *globe, stru_MBInfo *mb_info, UInt8 mode)
{
	stru_Result		*result;
	UInt8			i;
	Char			index[5];
	
	if (globe->result_head.next != (void *)&globe->result_tail && mode != SelectByEnterKey) //光标或选字键选字，且有结果
	{
		if (GetNearlySelector(&selector, globe, globe->page_count - 1)) //用户选择的位置有结果
		{
			//回溯至当前页的起始
			RollBackResult(globe);
			//移动到用户选择的结果
			result = globe->result;
			i = 1;
			while (i != selector)
			{
				result = (stru_Result *)result->next;
				i = (i << 1);
			}
			//获取结果
			if ((mb_info->type == 1 && globe->in_create_word_mode && globe->created_word_count < 10) || (mb_info->type == 0 && (((result->length+1) >> 1) + globe->created_key) <= globe->key_buf.key_index))
			{ //自造词模式
				globe->in_create_word_mode = true; //自造词标志
				StrCopy(globe->created_word[globe->created_word_count].result, result->result); //结果
				globe->created_word[globe->created_word_count].length = result->length; //长度
				globe->created_word[globe->created_word_count].record_index = result->record_index; //记录号
				MemMove(globe->created_word[globe->created_word_count].index, result->index, 5); //键值
				globe->created_word[globe->created_word_count].offset = result->offset; //偏移量
				globe->created_word_count ++;
				if (mb_info->type == 1)
				{
					MemSet(globe->key_buf.key[0].content, 100, 0x00);
					globe->key_buf.key[0].length = 0;
				}
				else
				{
					globe->created_key += (result->length >> 1);
				}
			}
			else
			{ //返回结果
				//先完成自造词工作
				if (globe->created_word_count < 10)
				{
					StrCopy(globe->created_word[globe->created_word_count].result, result->result); //结果
					globe->created_word[globe->created_word_count].length = result->length; //长度
					globe->created_word[globe->created_word_count].record_index = result->record_index; //记录号
					MemMove(globe->created_word[globe->created_word_count].index, result->index, 5); //键值
					globe->created_word[globe->created_word_count].offset = result->offset; //偏移量
					globe->created_word_count ++;
				}
				if (mb_info->type == 1)
				{
					MemSet(globe->key_buf.key[0].content, 100, 0x00);
					globe->key_buf.key[0].length = 0;
				}
				else
				{
					globe->created_key += (result->length >> 1);
				}
				if (mb_info->type == 0 && globe->in_create_word_mode)
				{ //不规则码表完成自造词
					for (i = 0; i < globe->created_key; i ++)
					{
						StrCat(buf, globe->created_word[i].result);
					}
					//填写自造词至码表
					if (globe->db_file_ref==NULL)//不在卡上			
					{
						MemSet(index, 5, 0x00);
						SaveWord(index, buf, globe, mb_info);
					}
				}
				else
				{ //直接返回结果
					if(mode>0)//以词定字
					{
						UInt8 tmpOffset;
						if((result->length>>1)>=mode)
							tmpOffset=2*(mode-1);
						else
							tmpOffset=(result->length-2);
						StrNCopy(buf, result->result+tmpOffset, 2);						
					}
					else
						StrCopy(buf, result->result);
					//调整词频
					if (mb_info->frequency_adjust && globe->db_file_ref == NULL)
					{
						FixWordOffset(result, globe, fixModeNormal, false);
					}
				}
				//操作信息
				(*operation) = pimeExit;
			}
			return true;
		}
	}
	else if (mode == SelectByEnterKey) //回车键选字
	{
		//返回已经完成的自造词
		for (i = 0; i < globe->created_word_count; i ++)
		{
			StrCat(buf, globe->created_word[i].result);
			//操作信息
			(*operation) = pimeCreateWord;
		}
		if (globe->key_buf.key[0].length > 0)
		{
			//返回未完成的关键字
			for (i = globe->created_key; i <= globe->key_buf.key_index; i ++)
			{
				StrCat(buf, globe->key_buf.key[i].content);
				//操作信息
				(*operation) = pimeExit;
			}
		}
		return true;
	}
	
	return false;
}
//--------------------------------------------------------------------------
//移动选字光标
static void MoveResultCursor(stru_Globe *globe, UInt8 op)
{	
	if (globe->page_count > 0) //有结果
	{
		//回溯至本页第一个结果
		RollBackResult(globe);
		switch (op)
		{
			case cursorLeft:
			{
				switch (globe->cursor)
				{
					case 4:
					{
						globe->cursor = 2;
						break;
					}
					case 2:
					{
						globe->cursor = 0;
						break;
					}
					case 0:
					{
						if (globe->result_status[globe->page_count] & slot2)
						{
							globe->cursor = 1;
						}
						break;
					}
					case 1:
					{
						if (globe->result_status[globe->page_count] & slot4)
						{
							globe->cursor = 3;
						}
						break;
					}
				}
				break;
			}
			case cursorRight:
			{
				switch (globe->cursor)
				{
					case 3:
					{
						globe->cursor = 1;
						break;
					}
					case 1:
					{
						globe->cursor = 0;
						break;
					}
					case 0:
					{
						if (globe->result_status[globe->page_count] & slot3)
						{
							globe->cursor = 2;
						}
						break;
					}
					case 2:
					{
						if (globe->result_status[globe->page_count] & slot5)
						{
							globe->cursor = 4;
						}
						break;
					}
				}
				break;
			}
		}
	}
}
//--------------------------------------------------------------------------
//转换按键值
static WChar KeyTransfer(EventType *eventP, stru_Pref *pref)
{
	switch (pref->KBMode)
	{
		case KBModeTreo:
		{
			if (eventP->data.keyDown.modifiers & 0x0008)
			{
				if (eventP->data.keyDown.keyCode != 0)
				{
					return (WChar)eventP->data.keyDown.keyCode;
				}
				else
				{
					return eventP->data.keyDown.chr;
				}
			}
			else
			{
				return eventP->data.keyDown.chr;
			}
			break;
		}
		case KBModeExt:
		case KBModeExtFull:
		{
			return CharToLower(eventP->data.keyDown.chr);
			break;
		}
	}
	
	return chrNull;
}


//--------------------------------------------------------------------------
//检查所有关键字，若其中没有非有效字符，返回假，否则返回真
static Boolean KeyBufHasUnusedChar(stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16		i;
	UInt16		j;
	UInt16		used_char_length;
	
	used_char_length = StrLen(mb_info->used_char);
	if (mb_info->translate_offset > 0) //需要进行键值转换的码表
	{
		for (i = 0; i < used_char_length; i ++)
		{
			globe->cache[i] = *KeyTranslate(mb_info->used_char[i], mb_info->key_translate, GetTranslatedKey);
		}
	}
	else
	{
		StrCopy(globe->cache, mb_info->used_char);
	}
	//合法性判断
	for (i = 0; i <= globe->key_buf.key_index; i ++)
	{
		for (j = 0; j < globe->key_buf.key[i].length; j ++)
		{
			if (StrChr(globe->cache, globe->key_buf.key[i].content[j]) == NULL) //找到一个非有效字符
			{
				MemSet(globe->cache, used_char_length, 0x00);
				return true;
			}
		}
	}
	
	MemSet(globe->cache, used_char_length, 0x00);
	return false;
}
//--------------------------------------------------------------------------
//按键是否选字键，若是，返回对应的选字位置，否则返回0xFF
static UInt8 KeyIsSelector(WChar key, stru_Globe *globe, stru_Pref *pref)
{
	UInt8		i;
	
	if (key == pref->Selector[0] || key == pref->Selector2[0])
	{
		return (slot1 << globe->cursor);
	}
	else
	{
		for (i = 1; i < 5; i ++)
		{
			if ((key == pref->Selector[i] || key == pref->Selector2[i])) //匹配
			{
				return (slot1 << i);
			}
		}
	}
	
	return 0xFF;
}
//--------------------------------------------------------------------------
//把按键添加至关键字缓存，已处理返回真
static Boolean KeywordHandler(WChar new_key, UInt8 *operation, stru_Globe *globe, stru_Pref *pref, Char *buf)
{
	UInt16		key_cache_length;
	UInt16		sample_length;
	UInt32		read_size;
	Boolean		matched = false;
	Char		*key_syncopate;
	Char		*tmp = NULL;
	WChar		caseSyncopateKey = pref->SyncopateKey;
	
	if(new_key == pref->SyncopateKey)//切音键，设置新建关键字标记
	{
		if (pref->KBMode != KBModeExtFull)
		{
			if (globe->english_mode)
			{
				if (globe->key_buf.key_index <= 9 && globe->key_buf.key[globe->key_buf.key_index].length < 99)
				{ //容量足够，可以添加
					globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
					globe->key_buf.key[globe->key_buf.key_index].length ++;
				}
			}
			else if (globe->key_buf.key[globe->key_buf.key_index].length != 0 && (! (globe->new_key || (globe->key_buf.key_index == 9 && globe->key_buf.key[9].length == pref->curMBInfo.key_length))) && pref->curMBInfo.type == 0)
			{ //未设置过标记且按键缓存未满，且当前缓存非空，可以设置
				globe->new_key = true;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		switch (new_key)
		{
			case keySemiColon:		//;键或'键，在全键盘模式下新建关键字标记
			case keySingleQuote:
			{
				if (pref->KBMode == KBModeExtFull)
				{
					if (globe->english_mode)
					{
						if (globe->key_buf.key_index <= 9 && globe->key_buf.key[globe->key_buf.key_index].length < 99)
						{ //容量足够，可以添加
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							globe->key_buf.key[globe->key_buf.key_index].length ++;
						}
					}
					else if (globe->key_buf.key[globe->key_buf.key_index].length != 0 && (! (globe->new_key || (globe->key_buf.key_index == 9 && globe->key_buf.key[9].length == pref->curMBInfo.key_length))) && pref->curMBInfo.type == 0)
					{ //未设置过标记且按键缓存未满，且当前缓存非空，可以设置
						globe->new_key = true;
					}
				}
				else
				{
					return false;
				}
				break;
			}
			case keyBackspace: //退格键，删除新建关键字标记，或从关键字中删除一个字符
			{
				if (globe->new_key) //设置过新建关键字标记，取消它
				{
					globe->new_key = false;
				}
				else if ((globe->key_buf.key_index >= 0 && globe->key_buf.key[0].length > 0) || (globe->in_create_word_mode && pref->curMBInfo.type == 1)) //有关键字内容，可以进行删除操作
				{
					if (globe->in_create_word_mode) //自造词模式
					{
						if (globe->created_word_count > 0)
						{
							//自造词缓存计数减一
							globe->created_word_count --;
							//恢复已完成的自造词计数
							if (pref->curMBInfo.type == 0)
							{
								globe->created_key -= (globe->created_word[globe->created_word_count].length >> 1);
							}
							//清除自造词缓存
							MemSet(globe->created_word[globe->created_word_count].result, 50, 0x00);
							globe->created_word[globe->created_word_count].length = 0;
							globe->created_word[globe->created_word_count].record_index = 0;
							MemSet(globe->created_word[globe->created_word_count].index, 5, 0x00);
							globe->created_word[globe->created_word_count].offset = 0;
							//若没有已完成的自造词了，退出造词模式
							if (globe->created_word_count == 0)
							{
								globe->in_create_word_mode = false;
							}
						}
						else if (globe->key_buf.key[0].length > 0) //规则码表在造词模式中删除关键字
						{
							globe->key_buf.key[globe->key_buf.key_index].length --;
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = 0x00;
						}
						else //退出造词模式
						{
							globe->in_create_word_mode = false;
						}
					}
					else if (globe->key_buf.key[0].length > 0)
					{
						globe->key_buf.key[globe->key_buf.key_index].length --;
						globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = 0x00;
						if (globe->key_buf.key[globe->key_buf.key_index].length == 0)
						{
							if (globe->key_buf.key_index > 0)
							{
								globe->key_buf.key_index --;
							}
							else
							{
								(*operation) = pimeExit;
							}
						}
					}
					globe->english_mode = KeyBufHasUnusedChar(globe, &pref->curMBInfo);
				}
				else //没有任何可以删除的内容，返回“假”
				{
					(*operation) = pimeExit;
				}
				break;
			}
			default: //把合法的键值添加至缓存
			{
				if (new_key >= 33 && new_key <= 126) //可视字符
				{
					if (new_key == keyComma && pref->KBMode == KBModeExtFull)
					{
						return false;
					}else if(pref->extractChar && new_key >=keyOne && new_key <= keyNine)//数字键 以词定字
					{
						return false;
					}
					if (StrChr(pref->curMBInfo.used_char, new_key) != NULL && (! globe->english_mode)) //中文模式，合法键值
					{
						if (globe->new_key || globe->key_buf.key[globe->key_buf.key_index].length == pref->curMBInfo.key_length)
						{ //有新建关键字标记，或当前一个缓存已满
							if (pref->curMBInfo.type == 0) //增加关键字
							{
								if (globe->key_buf.key_index < 9) //存在下一个可用缓存
								{
									globe->key_buf.key_index ++; //新建一个缓存
									globe->new_key = false; //取消新建关键字标记
								}
								else //无法再添加键值了，返回假
								{
									return false;
								}
							}
							else /*if( pref->autoSend)*/	//达至码长，自动上字并重新激活
							{
								SelectResult(buf, operation, slot1, globe, &pref->curMBInfo, SelectBySelector);
								if (! globe->in_create_word_mode)
								{
									(*operation) = pimeReActive;
								}
							}
						}
						//键值修正
						if (pref->curMBInfo.translate_offset != 0) //存在键值转换
						{
							//取键值对应的内容
							tmp = KeyTranslate((Char)new_key, pref->curMBInfo.key_translate, GetTranslatedKey);
							if (tmp != NULL) //找到匹配的内容
							{
								new_key = (WChar)(*tmp);
							}
						}
						//添加键值到缓存
						if (pref->curMBInfo.syncopate_offset != 0) //存在自动切音
						{
							//构造准备切音的关键字序列
							StrCopy(globe->cache, globe->key_buf.key[globe->key_buf.key_index].content);
							globe->cache[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							key_cache_length = globe->key_buf.key[globe->key_buf.key_index].length + 1; //序列长度
							key_syncopate = pref->curMBInfo.key_syncopate;
							read_size = 0;
							//循环匹配自动切音样本
							while (read_size < pref->curMBInfo.syncopate_size)
							{
								sample_length = StrLen(key_syncopate); //当前切音音节长度
								//采用后序最大匹配法，取偏移量
								if (key_cache_length >= sample_length)
								{
									tmp = globe->cache + (key_cache_length - sample_length);
								}
								else
								{
									tmp = globe->cache;
								}
								//与切音音节长度进行比较
								if (StrNCompare(tmp, key_syncopate, StrLen(tmp)/*StrLen(key_syncopate)*/) == 0) //匹配，进行音节划分处理
								{
									if (key_cache_length <= sample_length) //不需要划分
									{
										StrCopy(globe->key_buf.key[globe->key_buf.key_index].content, tmp);
										globe->key_buf.key[globe->key_buf.key_index].length ++;
									}
									else if (globe->key_buf.key_index < 9) //需要划分，且尚存在可分配的新缓存
									{
										//构造新音节
										StrCopy(globe->key_buf.key[globe->key_buf.key_index + 1].content, tmp);
										globe->key_buf.key[globe->key_buf.key_index + 1].length = StrLen(tmp);
										//切断
										(*tmp) = 0x00;
										//构造原音节
										StrCopy(globe->key_buf.key[globe->key_buf.key_index].content, globe->cache);
										globe->key_buf.key[globe->key_buf.key_index].length = StrLen(globe->cache);
										globe->key_buf.key_index ++;
									}
									else //虽然需要划分，但没有可分配的新缓存
									{
										StrCopy(globe->key_buf.key[globe->key_buf.key_index].content, tmp);
										globe->key_buf.key[globe->key_buf.key_index].length ++;
									}
									matched = true; //已匹配的标志
									break;
								}
								key_syncopate += sample_length + 1;
								read_size += sample_length + 1;
							}
							if ((! matched)) //尚未匹配，找不到匹配的组合
							{
								if (globe->key_buf.key_index < 9)
								{
									if (globe->key_buf.key[globe->key_buf.key_index].length > 0) //不是第一个码元，放置到新缓存中
									{
										globe->key_buf.key_index ++;
									}
									globe->key_buf.key[globe->key_buf.key_index].content[0] = (Char)new_key;
									globe->key_buf.key[globe->key_buf.key_index].length = 1;
								}
								else if (globe->key_buf.key[9].length < 99)
								{
									globe->key_buf.key[9].content[globe->key_buf.key[9].length] = (Char)new_key;
									globe->key_buf.key[9].length ++;
								}
							}
							//清除缓存
							MemSet(globe->cache, 128, 0x00);
						}
						else //不存在，直接添加
						{
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							globe->key_buf.key[globe->key_buf.key_index].length ++;
						}
					}
					else //置英文状态
					{
						if (globe->key_buf.key_index <= 9 && globe->key_buf.key[globe->key_buf.key_index].length < 99)
						{ //容量足够，可以添加
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							globe->key_buf.key[globe->key_buf.key_index].length ++;
							//英文模式
							globe->english_mode = true;
							//清空链表
							InitMBRecord(globe);
							InitResult(globe);
							//翻页标记
							globe->no_prev = true;
							globe->no_next = true;
						}
					}
				}
				else //非法字符
				{
					return false;
				}
				break;
			}
		}
	}
	return true;
}
//--------------------------------------------------------------------------
//获取卡上的码表的指针
static FileRef DmOpenDatabaseOnCard(stru_MBInfo *mb_info, stru_Globe *globe)
{
	UInt16				vol_ref;
	UInt32				vol_iterator = vfsIteratorStart;
	FileRef				file_ref = 0;
	
	//取卡指针
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref > 0) //卡存在
	{
		//构造完整路径
		StrCopy(globe->cache, PIME_CARD_PATH);
		StrCat(globe->cache, mb_info->file_name);
		VFSFileOpen(vol_ref, globe->cache, vfsModeRead, &file_ref);
		MemSet(globe->cache, 50, 0x00);
	}
	
	return file_ref;
}
//--------------------------------------------------------------------------
//创建输入框
static WinHandle CreateIMEForm(FormType **ime_form, RectangleType *ime_form_rectangle, stru_Globe *globeP)
{
	PointType		inspt_position;
	WinHandle		offset_buf = NULL;
	UInt16			err;
	Coord			extenty;
	
	InsPtGetLocation(&inspt_position.x, &inspt_position.y); //取光标坐标
	
	globeP->resultRect[0].topLeft.x = 63; globeP->resultRect[0].topLeft.y = 16; globeP->resultRect[0].extent.x = 30;
	globeP->resultRect[1].topLeft.x = 33; globeP->resultRect[1].topLeft.y = 16; globeP->resultRect[1].extent.x = 29;
	globeP->resultRect[2].topLeft.x = 94; globeP->resultRect[2].topLeft.y = 16; globeP->resultRect[2].extent.x = 29;
	globeP->resultRect[3].topLeft.x = 3; globeP->resultRect[3].topLeft.y = 16; globeP->resultRect[3].extent.x = 29;
	globeP->resultRect[4].topLeft.x = 124; globeP->resultRect[4].topLeft.y = 16; globeP->resultRect[4].extent.x = 29;
	
	if (globeP->settingP->displayFont == largeFont || globeP->settingP->displayFont == largeBoldFont)
	{
		if (globeP->settingP->curMBInfo.type == 1 && globeP->settingP->curMBInfo.gradually_search)	//逐码提示
		{
			if (globeP->settingP->shouldShowfloatBar)
				inspt_position.y += (inspt_position.y > 93)? -56 : 11;
			else
				inspt_position.y = 104;
			ime_form_rectangle->extent.y = 53;
			extenty = 28;
		}
		else
		{
			if (globeP->settingP->shouldShowfloatBar)
				inspt_position.y += (inspt_position.y > 98)? -51 : 11;
			else
				inspt_position.y = 109;			
			ime_form_rectangle->extent.y = 48;
			extenty = 32;
		}
	}
	else
	{
		if (globeP->settingP->curMBInfo.type == 1 && globeP->settingP->curMBInfo.gradually_search)	//逐码提示
		{
			if (globeP->settingP->shouldShowfloatBar)
				inspt_position.y += (inspt_position.y > 99)? -55 : 11;
			else
				inspt_position.y = 110;			
			ime_form_rectangle->extent.y = 47;
			extenty = 22;
		}
		else
		{
			if (globeP->settingP->shouldShowfloatBar)
				inspt_position.y += (inspt_position.y > 104)? -45 : 11;
			else
				inspt_position.y = 115;		
			ime_form_rectangle->extent.y = 42;			
			extenty = 26;
		}
	}
	(*ime_form) = FrmNewForm(frmIMEForm, NULL, 2, inspt_position.y, 156, ime_form_rectangle->extent.y + 2, true, NULL, NULL, NULL);
	offset_buf = WinCreateOffscreenWindow(156, ime_form_rectangle->extent.y + 2, nativeFormat, &err);
	globeP->resultRect[0].extent.y = extenty;
	globeP->resultRect[1].extent.y = extenty;
	globeP->resultRect[2].extent.y = extenty;
	globeP->resultRect[3].extent.y = extenty;
	globeP->resultRect[4].extent.y = extenty;	
	globeP->oneResultRect = globeP->resultRect[0];
	ime_form_rectangle->topLeft.x = 1;
	ime_form_rectangle->topLeft.y = 1;
	ime_form_rectangle->extent.x = 154;
	if (globeP->settingP->choice_button)
	{
		CtlNewControl((void **)ime_form, btnChrUP, buttonCtl, "\1", 124, 3, 15, 10, symbol7Font, 0, false);
		CtlNewControl((void **)ime_form, btnChrDOWN, buttonCtl, "\2", 140, 3, 15, 10, symbol7Font, 0, false);
	}
	if (globeP->settingP->menu_button)
	{
		CtlNewControl((void **)ime_form, btnChrMENU, buttonCtl, NULL, 2, 3, StrLen(globeP->settingP->curMBInfo.name)*6, 10, stdFont, 0, false);
	}
	return offset_buf;
}

#pragma mark -
//----------------------------------------------------------------
static Boolean keyHandleEvent(EventType *eventP, stru_Globe *globeP, WChar *chrP, UInt8 *operationP)
{
	Boolean		isKeyHandled		= false;
	Boolean		shouldRedrawForm	= false;
	
	if (eventP->eType == keyDownEvent && (! globeP->in_menu))
	{
		WChar	chr		= KeyTransfer(eventP, globeP->settingP);
		
		if ((! hasOptionPressed(eventP->data.keyDown.modifiers, globeP->settingP)) &&
			chr != vchrRockerCenter)
		{
			UInt8	i = KeyIsSelector(chr, globeP, globeP->settingP);
			if (i != 0xFF)
			{
				if (SelectResult(globeP->bufP, operationP, i, globeP, &globeP->settingP->curMBInfo, SelectBySelector))
				{
					if ((*operationP) == 0xFF) //选字完成后未指示退出输入框
					{
						if (globeP->in_create_word_mode) //处于自造词模式，且造词未完成
						{
							//检索码表
							SearchMB(globeP, &globeP->settingP->curMBInfo);
						}
						
						shouldRedrawForm = true;
					}
					
					isKeyHandled = true;
				}
			}
		}
		if (! isKeyHandled)
		{
			//会引起重新查找码表的操作
			if (KeywordHandler(chr, operationP, globeP, globeP->settingP, globeP->bufP))
			{
				if ((*operationP) == 0xFF)
				{
					if (! globeP->english_mode)
					{
						//检索码表
						SearchMB(globeP, &globeP->settingP->curMBInfo);
					}
					
					shouldRedrawForm = true;
					
					/*//是否达到自动上字的条件
					if ((! globeP->in_create_word_mode) && globeP->settingP->curMBInfo.type == 1 && globeP->key_buf.key[0].length == globeP->settingP->curMBInfo.key_length && HasOnlyOneResult(globeP))
					{ //达到四码自动上字的条件，自动上字
						SelectResult(globeP->bufP, operationP, slot1, globeP, &globeP->settingP->curMBInfo, SelectBySelector);
					}*/
					isKeyHandled = true;
				}
				else if ((*operationP) == pimeReActive)
				{
					globeP->initKey = chr;
				}
			}
			
			if (! isKeyHandled)
			{
				if(chr == globeP->settingP->MenuKey)
				{
					if ((! globeP->in_create_word_mode) && globeP->db_file_ref == NULL)//自定义按键激活输入框菜单
						{ //有option组合或menu键不是选字键，且不处于自造词模式
							globeP->imeMenuP = MenuGetActiveMenu(); //获取当前菜单
							if (globeP->imeMenuP == NULL) //菜单未加载，加载它
							{
								globeP->imeMenuP = MenuInit(menuSpecial);
								MenuSetActiveMenu(globeP->imeMenuP);
								if (globeP->settingP->curMBInfo.type == 0) //不规则码表，屏蔽手动造词选项
								{
									MenuHideItem(miCreateWord);
								}
								/*if (StrNCaselessCompare(globeP->settingP->curMBInfo.file_name + (StrLen(globeP->settingP->curMBInfo.file_name)-3), "GBK", 3) != 0) //GB码表，屏蔽字符文件集选项
								{
									MenuHideItem(miGB);
									MenuHideItem(miGBK);
								}*/
							}
							MenuDrawMenu(globeP->imeMenuP); //显示菜单
							globeP->in_menu = true; //菜单打开标识
							isKeyHandled = true;
						}
				}
				else
				{
					switch (chr)
					{
						case vchrMenu: //激活输入框菜单
						{
							if ((! globeP->in_create_word_mode) && globeP->db_file_ref == NULL)
							{ //有option组合或menu键不是选字键，且不处于自造词模式
								globeP->imeMenuP = MenuGetActiveMenu(); //获取当前菜单
								if (globeP->imeMenuP == NULL) //菜单未加载，加载它
								{
									globeP->imeMenuP = MenuInit(menuSpecial);
									MenuSetActiveMenu(globeP->imeMenuP);
									if (globeP->settingP->curMBInfo.type == 0) //不规则码表，屏蔽手动造词选项
									{
										MenuHideItem(miCreateWord);
									}
									/*if (StrNCaselessCompare(globeP->settingP->curMBInfo.file_name + (StrLen(globeP->settingP->curMBInfo.file_name)-3), "GBK", 3) != 0) //GB码表，屏蔽字符文件集选项
									{
										MenuHideItem(miGB);
										MenuHideItem(miGBK);
									}*/
								}
								MenuDrawMenu(globeP->imeMenuP); //显示菜单
								globeP->in_menu = true; //菜单打开标识
								isKeyHandled = true;
							}
							break;
						}
						case chrUpArrow: //上翻页
						case vchrPageUp:
						case vchrRockerUp:
						case keyComma: //101键上翻页
						{
							if (! globeP->no_prev && (chr==keyComma ? globeP->settingP->KBMode == KBModeExtFull: true)) //可以上翻
							{
								//回溯至上一页的第一个结果
								RollBackResult(globeP); //当前页
								RollBackResult(globeP); //上一页
								globeP->cursor = 0;
								
								shouldRedrawForm = true;
								isKeyHandled = true;
							}
							break;
						}
						case chrDownArrow: //下翻页
						case vchrPageDown:
						case vchrRockerDown:
						case keyPeriod:  //101键下翻页
						{
							if (! globeP->no_next && (chr==keyPeriod ? globeP->settingP->KBMode == KBModeExtFull: true)) //可以下翻
							{
								globeP->no_prev = false;
								globeP->cursor = 0;
								
								shouldRedrawForm = true;
								isKeyHandled = true;
							}
							break;
						}
						case keyReturn: //规则编码手动造词，或返回英文
						{
							SelectResult(globeP->bufP, operationP, 0, globeP, &globeP->settingP->curMBInfo, SelectByEnterKey);
							
							isKeyHandled = true;
							break;
						}
						case chrLeftArrow: //选字光标往左
						case vchrRockerLeft:
						{
							if(!globeP->english_mode)//英文模式下无候选字
								MoveResultCursor(globeP, cursorLeft);
							
							shouldRedrawForm = true;
							isKeyHandled = true;
							break;
						}
						case chrRightArrow: //选字光标往右
						case vchrRockerRight:
						{
							if(!globeP->english_mode)
								MoveResultCursor(globeP, cursorRight);
							
							shouldRedrawForm = true;
							isKeyHandled = true;
							break;
						}
						case keyOne:
						case keyTwo:
						case keyThree:
						case keyFour:
						case keyFive:
						case keySix:
						case keySeven:
						case keyEight:
						case keyNine: //以词定字
						{
							SelectResult(globeP->bufP, operationP, (slot1 << globeP->cursor), globeP, &globeP->settingP->curMBInfo, chr-keyZero);
							if ((*operationP) == 0xFF && globeP->in_create_word_mode) //处于自造词模式，且造词未完成
							{
								//检索码表
								SearchMB(globeP, &globeP->settingP->curMBInfo);
								
								shouldRedrawForm = true;
							}
							isKeyHandled = true;
							break;
						}
						case vchrRockerCenter:
						case vchrHardRockerCenter: //光标选字
						{
							if ((! (eventP->data.keyDown.modifiers & willSendUpKeyMask)) || globeP->settingP->isTreo != isTreo650)
							{
								SelectResult(globeP->bufP, operationP, (slot1 << globeP->cursor), globeP, &globeP->settingP->curMBInfo, SelectBySelector);
								if ((*operationP) == 0xFF && globeP->in_create_word_mode) //处于自造词模式，且造词未完成
								{
									//检索码表
									SearchMB(globeP, &globeP->settingP->curMBInfo);
									
									shouldRedrawForm = true;
								}
							}
							isKeyHandled = true;
							break;
						}
					}
				}
			}
		}
	}
	if (shouldRedrawForm)
	{
		FrmUpdateForm(frmIMEForm, frmRedrawUpdateCode);
	}
	/*else
	{
		FrmUpdateForm(frmIMEForm, FORM_UPDATE_FRAMEONLY);
	}*/
	
	return isKeyHandled;
}
static Boolean appHandleEvent(EventType *eventP, stru_Globe *globeP, UInt8 *operationP)
{
	Boolean		isEventHandled		= false;
	Boolean		shouldRedrawForm	= false;
	
	switch (eventP->eType)
	{
		case frmOpenEvent:	//打开输入框
		{
			if (eventP->data.frmOpen.formID == frmIMEForm)
			{
				//打开输入法窗口
				globeP->draw_buf = CreateIMEForm(&globeP->imeFormP, &globeP->imeFormRectangle, globeP);
				if(globeP->settingP->showGsi) FrmNewGsi(&globeP->imeFormP, globeP->settingP->choice_button?114:132, 3);
				else FrmNewGsi(&globeP->imeFormP, 160, 160);				
				FrmSetActiveForm(globeP->imeFormP);				
				FrmDrawForm(globeP->imeFormP);							
				GsiEnable(true);
				GrfInitState();
				
				globeP->imeFormP = FrmGetActiveForm();	//刷新一遍窗体指针
				
				
				//处理初始按键
				if (KeywordHandler(globeP->initKey, operationP, globeP, globeP->settingP, globeP->bufP))
				{
					if ((*operationP) == 0xFF)
					{
						if (! globeP->english_mode)
						{
							//检索码表
							SearchMB(globeP, &globeP->settingP->curMBInfo);
						}
						
						//是否达到自动上字的条件
						if ((! globeP->in_create_word_mode) && globeP->settingP->curMBInfo.type == 1 && globeP->key_buf.key[0].length == globeP->settingP->curMBInfo.key_length && HasOnlyOneResult(globeP))
						{ //达到四码自动上字的条件，自动上字
							SelectResult(globeP->bufP, operationP, slot1, globeP, &globeP->settingP->curMBInfo, SelectBySelector);
						}
					}
				}
				globeP->initKey = 0;
				shouldRedrawForm = true;
				isEventHandled = true;
			}
			break;
		}
		case frmUpdateEvent:
		{
			if (eventP->data.frmUpdate.formID == frmIMEForm)
			{					
				DrawIMEForm(globeP->imeFormP, &globeP->imeFormRectangle, globeP, globeP->settingP, eventP->data.frmUpdate.updateCode);
				//是否达到自动上字的条件
				if (globeP->settingP->autoSend && (! globeP->in_create_word_mode) && globeP->settingP->curMBInfo.type == 1 && globeP->key_buf.key[0].length == globeP->settingP->curMBInfo.key_length && HasOnlyOneResult(globeP))
				{ //达到四码自动上字的条件，自动上字
					SelectResult(globeP->bufP, operationP, slot1, globeP, &globeP->settingP->curMBInfo, SelectBySelector);
				}				
				isEventHandled = true;
			}
			break;
		}
		case winEnterEvent: //是否退出菜单
		{
			if (globeP->imeFormP)
			{
				if (globeP->in_menu && eventP->data.winEnter.enterWindow == (WinHandle)globeP->imeFormP)
				{ //是从菜单退出
					globeP->in_menu = false;
					isEventHandled = true;
				}		
			}
			break;
		}
		case menuEvent: //菜单事件
		{
			switch (eventP->data.menu.itemID)
			{
				case miCreateWord: //打开规则码表的自造词模式
				{
					//设置标识
					globeP->in_create_word_mode = true;
					//重绘输入框
					if (globeP->result_head.next != (void *)&globeP->result_tail)
					{
						RollBackResult(globeP);
					}
					break;
				}
				case miDeleteWord: //删除词组
				{
					if (DeleteWord(globeP)) //重新检索码表
					{
						SearchMB(globeP, &globeP->settingP->curMBInfo);
						globeP->cursor = 0;
					}
					else
					{
						RollBackResult(globeP);
					}
					break;
				}
				case miMoveAhead: //强制前置词组
				{
					if (MoveWordToTop(globeP, false)) //强制提前
					{
						SearchMB(globeP, &globeP->settingP->curMBInfo);
						globeP->cursor = 0;
					}
					else
					{
						RollBackResult(globeP);
					}
					break;
				}
				case miSetStatic: //固顶字词
				{
					if (MoveWordToTop(globeP, true)) //强制提前并固顶
					{
						SearchMB(globeP, &globeP->settingP->curMBInfo);
						globeP->cursor = 0;
					}
					else
					{
						RollBackResult(globeP);
					}
					break;
				}
				case miUnsetStatic: //取消固顶
				{
					UnsetStaticWord(globeP);
					SearchMB(globeP, &globeP->settingP->curMBInfo);
					globeP->cursor = 0;
					break;
				}
			}
			shouldRedrawForm = true;
			isEventHandled = true;
			break;
		}
		case penDownEvent: //触屏关闭输入框或选字
		{
			if ((! globeP->in_menu))
			{
				if (! RctPtInRectangle(eventP->screenX, eventP->screenY, &globeP->imeFormRectangle))//框外退出
				{
					(*operationP) = pimeExit;
					isEventHandled = true;
				}				
				else
				{
					UInt16		i;
					
					for (i = 0; i < 5; i ++)
					{
						if (RctPtInRectangle(eventP->screenX, eventP->screenY, &globeP->resultRect[i]))
						{
							EvtEnqueueKey(globeP->settingP->Selector[i], 0, 0);
							isEventHandled = true;
							break;
						}
					}
				}
				if(!isEventHandled)
				{
					RectangleType R2 = {40, 2, 80, 12}; 
					if(RctPtInRectangle(eventP->screenX, eventP->screenY, &R2))//修改编码
					{
						UInt16 i, key_lengths=0;
						for(i=0;i<=globeP->key_buf.key_index;i++)
						{
							EnqueueResultToKey(globeP->key_buf.key[i].content, globeP->key_buf.key[i].length);
							key_lengths+=globeP->key_buf.key[i].length;
							if(i<globeP->key_buf.key_index)
								EvtEnqueueKey(chrFullStop, 0, 0);
						}
						key_lengths+=globeP->key_buf.key_index;
						MemSet(globeP->cache, 512, 0x00);
						if(eventP->screenX<77)//是否将光标定位在最左侧
						{
							for(i=0;i<key_lengths;i++)
								EvtEnqueueKey(chrLeftArrow, 0, 0);
						}
						FrmCustomResponseAlert (alertInput, "Enter", "new", "code", globeP->cache, 512, NULL);
						if(globeP->cache[0])//保存新音节
						{
							Char *tmp=globeP->cache;
							UInt16 j=0;
							i=0;
							MemSet(globeP->key_buf.key[j].content, 100, 0x00);
							while(*tmp)
							{
								if(*tmp==chrFullStop)
								{
									globeP->key_buf.key[j].content[i]=chrNull;
									globeP->key_buf.key[j].length=i;
									j++;
									MemSet(globeP->key_buf.key[j].content, 100, 0x00);
									i=0;
								}
								else
								{
									globeP->key_buf.key[j].content[i++]=*tmp;
								}									
								tmp++;
							}
							if(i>0)
							{
								globeP->key_buf.key[j].content[i]=chrNull;
								globeP->key_buf.key[j].length=i;
								globeP->key_buf.key_index=j;
							}
							else
								globeP->key_buf.key_index=j-1;
							MemSet(globeP->cache, 512, 0x00);
							SearchMB(globeP, &globeP->settingP->curMBInfo);
							shouldRedrawForm = true;						
						}												
						isEventHandled = true;
					}
				}
			}
			break;
		}
		case ctlSelectEvent: //按钮
		{
			switch (eventP->data.ctlSelect.controlID)
			{
				case btnChrUP:
				{
					EvtEnqueueKey(chrUpArrow, 0, 0);
					break;
				}
				case btnChrDOWN:
				{
					EvtEnqueueKey(chrDownArrow, 0, 0);
					break;
				}
				case btnChrMENU:
				{
					globeP->imeMenuP = MenuGetActiveMenu(); //获取当前菜单
					if (globeP->imeMenuP == NULL) //菜单未加载，加载它
					{
						globeP->imeMenuP = MenuInit(menuSpecial);
						MenuSetActiveMenu(globeP->imeMenuP);
						if (globeP->settingP->curMBInfo.type == 0) //不规则码表，屏蔽手动造词选项
						{
							MenuHideItem(miCreateWord);
						}
					}
					MenuDrawMenu(globeP->imeMenuP); //显示菜单
					globeP->in_menu = true; //菜单打开标识
					break;
				}
				/*default:
				{
					EvtEnqueueKey(globeP->settingP->Selector[eventP->data.ctlSelect.controlID - 0x1B59], 0, 0);
				}*/
			}
			isEventHandled = true;
		}
	}
	
	if (shouldRedrawForm)
	{
		FrmUpdateForm(frmIMEForm, frmRedrawUpdateCode);
	}
	
	return isEventHandled;
}

#pragma mark -

/********************************************************************
*
* 函数名:    SLWinDrawBitmap
*
*   描述:    绘制资源索引/ID对应的位图
*
* 返回值:    无
*
*   历史:	姓  名		日  期			描        述
*			------		----------		-----------------
*			Sean		2003/08/14		初始版本
*			Bob			2008/07/03		修改了x,y定义
*******************************************************************/
void SLWinDrawBitmap
(
        DmOpenRef dbP,        // (in)资源文件数据库指针
        UInt16 uwBitmapIndex, // (in)位图资源的Index或者ID
        Coord x,              // (in)位图距离右下角的x坐标
        Coord y,              // (in)位图距离右下角的y坐标
        Boolean bByIndex      // (in)true：根据资源索引来获取Bitmap
                              //     false：根据资源ID来获取Bitmap
                              // 如果为true，将忽略dbP参数
)
{
        UInt32        udwWinVersion;
        Boolean       bHiRes;
       
        MemHandle     resourceH;
        BitmapPtr     resourceP;
        UInt16        uwPrevCoord;
        MemHandle     bmpH=0;
        BitmapPtr     bmpP;
        RectangleType typRect;
        WinHandle     winH, oldWinH;
        BitmapType    *bitmapP;
        Coord         wWidth, wHeight;
        Err           err;
        Coord			extentX,extentY;
       
		//WinGetDisplayExtent (&extentX, &extentY);
		WinGetWindowExtent (&extentX, &extentY);
		
		x = extentX - x;
		y = extentY - y;
       
        if (bByIndex)
        {
                if (dbP == NULL) return;
        }       
       
        // 获取位图资源
        resourceP = NULL;
        if (bByIndex)
        {
                resourceH = DmGetResourceIndex(dbP, uwBitmapIndex);
        }
        else
        {
                resourceH = DmGetResource(bitmapRsc, uwBitmapIndex);
        }
        ErrFatalDisplayIf(! resourceH, "Cannot open the bitmap.");
        resourceP = (BitmapPtr)MemHandleLock(resourceH);

        // 判断Windows Manager的版本
        FtrGet(sysFtrCreator, sysFtrNumWinVersion, &udwWinVersion);
        if (udwWinVersion >= 4)
        {
                bHiRes = true;
        }
        else
        {
                bHiRes = false;
        }

        // 如果不是高分辨率，直接绘制
        if (! bHiRes)
        {
                WinDrawBitmap (resourceP, x, y);
                MemPtrUnlock(resourceP);
                DmReleaseResource(resourceH);
                return;
        }


        // 判断位图是否是高分的，如果不是，直接绘制
        if (BmpGetDensity(resourceP) == kDensityLow)
        {
                WinDrawBitmap (resourceP, x, y);
                MemPtrUnlock(resourceP);
                DmReleaseResource(resourceH);
                return;
        }
       
        // 下面采用高分辨率进行绘制
        // 设置Native坐标系
        uwPrevCoord = WinSetCoordinateSystem(kCoordinatesNative);

        // 先在虚拟窗口中绘图，采用低分辨率绘制
        bitmapP = NULL;
        bmpP = NULL;
       
        winH = WinCreateOffscreenWindow(320, 320, nativeFormat, &err);
        if (err)
        {
                // 恢复坐标系
                WinSetCoordinateSystem(uwPrevCoord);       
                return;
        }
       
        bitmapP = WinGetBitmap(winH);
        BmpSetDensity(bitmapP, kDensityLow);
        oldWinH = WinSetDrawWindow(winH);
        bmpH = DmGetResourceIndex(dbP, uwBitmapIndex);
        ErrFatalDisplayIf(! bmpH, "Cannot open the bitmap.");
        bmpP = (BitmapPtr)MemHandleLock(bmpH);
       
        // 获取图像大小
        BmpGetDimensions(bmpP, &wWidth, &wHeight, 0);
        WinDrawBitmap(bmpP, 0, 0);

        typRect.topLeft.x = 0;
        typRect.topLeft.y = 0;
        typRect.extent.x = wWidth;
        typRect.extent.y = wHeight;

        MemHandleUnlock(bmpH);
        DmReleaseResource(bmpH);

        // 复制到原来的窗口，以高分辨率绘制
        BmpSetDensity(bitmapP, kDensityDouble);
        WinSetDrawWindow(oldWinH);
        WinCopyRectangle(winH, 0, &typRect, x, y, winPaint);
        WinDeleteWindow(winH,0);
       
        // 恢复坐标系
        WinSetCoordinateSystem(uwPrevCoord);       
}
//--------------------------------------------------------------------------
//输入框事件处理
static UInt8 PIMEEventHandler(WChar *chrP, Char *bufP, stru_Pref *settingP, FormType *curFormP)
{
	EventType		event;
	UInt8			operation = 0xFF;
	UInt16			error;
	stru_Globe		*globeP;
	Char 			bufK[5]=""; 
	
	//初始化变量
	globeP = (stru_Globe *)MemPtrNew(sizeof(stru_Globe));
	MemSet(globeP, sizeof(stru_Globe), 0x00);
	globeP->settingP = settingP;
	globeP->bufP = bufP;
	globeP->initKey = (*chrP);
	globeP->result_head.next = (void *)&globeP->result_tail;
	globeP->result_tail.prev = (void *)&globeP->result_head;
	
	FntSetFont(settingP->displayFont);
	globeP->curCharWidth = FntCharsWidth("中", 2);
	globeP->curCharHeight = FntCharHeight();
	FntSetFont(stdFont);
	
	//打开数据库
	if (settingP->curMBInfo.inRAM || settingP->dync_load) //码表在内存
	{
		globeP->db_ref = DmOpenDatabaseByTypeCreator(settingP->curMBInfo.db_type, appFileCreator, dmModeReadWrite);
	}
	else
	{
		globeP->db_file_ref = DmOpenDatabaseOnCard(&settingP->curMBInfo, globeP);
	}
	
	FrmPopupForm(frmIMEForm);
	
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		
		if (! keyHandleEvent(&event, globeP, chrP, &operation))
		{
			if (! SysHandleEvent(&event))
			{
				if (! MenuHandleEvent(0, &event, &error))
				{
					if (! appHandleEvent(&event, globeP, &operation))
					{
						if (globeP->imeFormP)
						{
							FrmDispatchEvent(&event);
						}
					}
				}
			}
		}
	}while (event.eType != appStopEvent && operation == 0xFF);
	
	//关闭数据库
	DmCloseDatabaseFromCardAndRAM(globeP->db_ref, globeP->db_file_ref);
	
	InitMBRecord(globeP);
	InitResult(globeP);
	WinDeleteWindow(globeP->draw_buf, false);
	//UIColorSetTableEntry(UIFormFill, &globeP->org_color);
	FrmReturnToForm(0);
	//ime_form = FrmGetActiveForm();
	//FrmEraseForm(globeP->imeFormP);
	//FrmDeleteForm(globeP->imeFormP);
	//FrmSetActiveForm(curFormP);
	if (operation == pimeCreateWord)
	{			
		GetWordCodes(globeP->settingP->curMBInfo.db_type, StrLen(bufP)/2, bufK, globeP->created_word_count, globeP->created_word);
		CreateWordEventHandler(bufP, bufK, settingP);
	}
	(*chrP) = globeP->initKey;
	MemPtrFree(globeP);
	
	return operation;
}
//--------------------------------------------------------------------------
//规则码表手动造词对话框
static void CreateWordEventHandler(Char *word, Char *key, stru_Pref *pref)
{
	FormType	*create_word_form = NULL;
	FieldType	*key_field;
	FieldType	*word_field;
	EventType	event;
	Boolean		handled;
	Char		index[5];
	//Char		*key;
	Char		*content;
	UInt16		i;
	UInt16		j = 0;
	UInt16		key_length;
	UInt16		word_length;
	Boolean		exit = false;
	stru_Globe	*globe;
	
	//初始化变量
	globe = (stru_Globe *)MemPtrNew(stru_Globe_length);
	MemSet(globe, stru_Globe_length, 0x00);
	globe->result_head.next = (void *)&globe->result_tail;
	globe->result_tail.prev = (void *)&globe->result_head;
	//打开数据库
	globe->db_ref = DmOpenDatabaseByTypeCreator(pref->curMBInfo.db_type, appFileCreator, dmModeReadWrite);
	//清空索引
	MemSet(index, 5, 0x00);
	//打开手动造词对话框
	FrmPopupForm(frmCreateWord);
	
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		handled = false;
		
		if (! SysHandleEvent(&event))
		{
			switch (event.eType)
			{
				case frmLoadEvent:
				{
					create_word_form = FrmInitForm(frmCreateWord);
					handled = true;
					break;
				}
				case frmOpenEvent:
				{
					key_field = (FieldType *)FrmGetObjectPtr(create_word_form, FrmGetObjectIndex(create_word_form, fldCode));
					word_field = (FieldType *)FrmGetObjectPtr(create_word_form, FrmGetObjectIndex(create_word_form, fldWord)); 
					FrmSetActiveForm(create_word_form);
					FrmDrawForm(create_word_form);
					//FrmSetFocus (create_word_form, FrmGetObjectIndex(create_word_form, fldCode));
					FldInsert(word_field, word, StrLen(word));
					FldInsert(key_field, key, StrLen(key));
					handled = true;
					break;
				}
				case ctlSelectEvent:
				{
					switch (event.data.ctlSelect.controlID)
					{
						case btnSaveWord: //保存键
						{
							key_length = FldGetTextLength(key_field); //获取关键字长度
							word = FldGetTextPtr(word_field);
							word_length = StrLen(word); //获取词组长度
							key = FldGetTextPtr(key_field); //获取关键字内容
							//关键字内没有万能键（合法）、键值长度符合码表要求且文字部分不是单字，保存词组
							if (KeyHasWildChar(key, key_length, &pref->curMBInfo) && key_length <= pref->curMBInfo.key_length && key_length > 0 && word_length > 2)
							{
								FrmCustomAlert(alertCreateWordErr, "Codes contains wild character.", "", "");
							}
							else
							{
								//构造完整的内容单元（不包括结尾标识\0x01，该部分在SaveWord()中添加）
								content = (Char *)MemPtrNew(key_length + word_length + 1); //分配内存
								StrCopy(content, key); //关键字
								StrCat(content, word); //词组
								//构造该词组的索引
								for (i = 0; i < key_length; i ++)
								{
									index[j] = key[i];
									if (j < 3)
									{
										j ++;
									}
								}
								SaveWord(index, content, globe, &pref->curMBInfo);
								MemPtrFree(content); //释放内存
								exit = true;
							}
							handled = true;
							break;
						}
						case btnCancel:
						{
							handled = true;
							exit = true;
							break;
						}
					}
					break;
				}
			}
			if (!handled)//(! (handled/* || create_word_form == NULL*/))
			{
				FrmDispatchEvent(&event);
			}
		}
	}
	while (event.eType != appStopEvent && (! exit));

	//关闭数据库
	DmCloseDatabase(globe->db_ref);
	//释放内存
	MemPtrFree(globe);
	//关闭对话框
	FrmReturnToForm(0);
}
//--------------------------------------------------------------------------
//转换按键为Treo键盘上的英文标点
static void TreoKBEnglishPunc(Char *str, WChar curKey)
{
	Char punc_str[27]="&#84156$@!:',?\"p/2-3)9+7(*"; //Treo键盘
	Int16 idx=curKey-keyA;
	if(idx>=0 && idx<=25)
		str[0]=(idx>=0 && idx<=25)? punc_str[idx]:(Char)curKey;
}

//--------------------------------------------------------------------------
//Treo键盘符号处理
static Char *TreoKBPuncEventHandler(WChar curKey, UInt16 curKeyCode, stru_Pref *pref, Boolean isLongPress)
{
	Char	*str = NULL;
	
	str = MemPtrNew(15);
	MemSet(str, 15, 0x00);
	
	if (! isLongPress)
	{
		if (! (pref->hasShiftMask || pref->hasOptionMask))
		{
			switch (curKey)
			{
				case keyPeriod: //句号
				{
					if (pref->english_punc)
					{
						StrCopy(str, ".");
					}
					else
					{
						StrCopy(str, "。");
					}
					break;
				}
			}
		}
		else if (pref->hasShiftMask)
		{
			switch (curKey)
			{
				case keyPeriod: //分号
				{
					//StrCopy(str, "；");
					StrCopy(str, pref->CustomLPShiftPeriod);
					break;
				}
				case keyBackspace: //破折号
				{
					//StrCopy(str, "——");
					StrCopy(str, pref->CustomLPShiftBackspace);
					break;
				}
			}
		}
		else if (pref->hasOptionMask)
		{
			switch (curKey)
			{
				case keyPeriod: //英文句号
				{
					StrCopy(str, ".");
					break;
				}
				case keyBackspace: //省略号
				{
					//StrCopy(str, "……");
					StrCopy(str, pref->CustomLPOptBackspace);
					break;
				}
				default:
					TreoKBEnglishPunc(str, curKey);	
			}
		}
	}
	else //长按
	{
		if (pref->isTreo == isTreo600 && curKeyCode == hsKeySymbol)
		{
			StrCopy(str, "0");
		}
		else if (! pref->english_punc)
		{
			Int16 idx = curKey - keyA;
			if(idx>=0 && idx<=25)
				StrCopy(str, pref->CustomLP[idx]);
			else if(curKey == keyPeriod)
				StrCopy(str, pref->CustomLPPeriod);
			else
				str[0] = (Char)curKey;
		}
		else
		{
			TreoKBEnglishPunc(str, curKey);		
		}
	}
	if(pref->fullwidth &&(pref->num_fullwidth || str[0]>'9' || str[0]<'0')/* 是否为全角数字*/)	
		TreoKBFullwidth(str);//全角符号
	TreoKBDynamicPunc(str);//命令符号
	return str;
}
//--------------------------------------------------------------------------
//外置键盘符号处理
static Char *ExtKBPuncEventHandler(WChar curKey, UInt16 curKeyCode, stru_Pref *pref, Boolean isLongPress)
{
	Char	*str = NULL;
	
	str = MemPtrNew(5);
	MemSet(str, 5, 0x00);
	
	switch (curKey)
	{
		case 33: //叹号
		{
			StrCopy(str, "！");
			break;
		}
		case 34: //双引号
		{
			StrCopy(str, "“”");
			break;
		}
		case 35: //波浪号
		{
			StrCopy(str, "～");
			break;
		}
		case 39: //单引号
		{
			StrCopy(str, "‘’");
			break;
		}
		case 40: //括号
		{
			StrCopy(str, "（）");
			break;
		}
		case 41: //书名号
		{
			StrCopy(str, "《》");
			break;
		}
		case 42: //顿号
		{
			StrCopy(str, "、");
			break;
		}
		case 43: //省略号
		{
			StrCopy(str, "……");
			break;
		}
		case 44: //逗号
		{
			StrCopy(str, "，");
			break;
		}
		case 45: //破折号
		{
			StrCopy(str, "——");
			break;
		}
		case 46: //句号
		{
			StrCopy(str, "。");
			break;
		}
		case 58: //冒号
		{
			StrCopy(str, "：");
			break;
		}
		case 63: //问号
		{
			StrCopy(str, "？");
			break;
		}
	}
	return str;
}
//--------------------------------------------------------------------------
//根据给定的位置，获取可以使用的内容起始位置
static UInt16 GetStartPosition(UInt16 position, Char *text)
{
	UInt16		text_length;
	
	text_length = StrLen(text);
	if (position >= text_length) //超出范围，修正
	{
		position = text_length - 1;
	}
	//从当前位置往前移动，直至找到分隔符“ ”，或到达文本头部
	while (text[position] != ' ' && position > 0)
	{
		position --;
	}
	if (position > 0) //属于找到分隔符后跳出的情况
	{
		position ++; //指向分隔符后的字符
	}
	return position;
}
//--------------------------------------------------------------------------
//根据给定的起始位置，获取被选择内容的结束位置
static UInt16 GetEndPosition(UInt16 startPosition, Char *text)
{
	UInt16		endPosition;
	
	endPosition = startPosition;
	while (text[endPosition] != '\0' && text[endPosition] != ' ')
	{
		endPosition ++;
	}
	
	return endPosition;
}
#pragma mark -
//--------------------------------------------------------------------------
static Boolean KeyTransfer2(WChar *key, EventType *event, stru_Pref *pref)
{
	
	if (event->eType == keyDownEvent)
	{
		switch (pref->KBMode)
		{
			case KBModeTreo:
			{
				if (EvtKeydownIsVirtual(event))
				{
					if (event->data.keyDown.keyCode > 0)
					{
						(*key) = (WChar)event->data.keyDown.keyCode;
					}
					else
					{
						return false;
					}
				}
				else
				{
					(*key) = CharToLower(event->data.keyDown.chr);
				}
				break;
			}
			case KBModeExt:
			case KBModeExtFull:
			{
				(*key) = CharToLower(event->data.keyDown.chr);
				break;
			}
		}
		return true;
	}
	else if ((*key) > 0)
	{
		return true;
	}
	
	return false;
}

//
//
void myDrawFunc(UInt16 itemNum, RectangleType *bounds, Char **itemsText)
{
       	Char *output, len; 
       	Boolean sep = false;               
        output=*(itemsText+itemNum);
        len=StrLen(output);
        if(len>1 && output[len-1]=='-')
        {
        	len--;
        	sep = (Boolean)itemNum;        	
        }	                 
        WinDrawTruncChars(output,len,
                bounds->topLeft.x,
                bounds->topLeft.y, 48);
        if(sep) 
	        WinDrawLine(bounds->topLeft.x-2,
		                        bounds->topLeft.y,
		                        bounds->extent.x+3,
		                        bounds->topLeft.y);  
}        
//
//获取字符的顺序编号
static UInt16 GetCharIndex(WChar curChar)
{
	UInt16 charIndex;
	UInt8 b0, b1;
	b0=*(UInt8 *)&curChar;
	b1=*((UInt8 *)(&curChar)+1);
    if (b0>=0xA1)
        if (b1>=0xA1)
            charIndex=(b0-0xA1)*94+(b1-0xA1);
        else
            charIndex=8836+(b0-0xA1)*(0xA0-0x40+1)+(b1-0x40);
    else
        charIndex=8836+9118+(b0-0x81)*(0xFE-0x40+1)+(b1-0x40);
    return charIndex;
}
//---------------------
//联想, 反查汉字信息事件
static void AltEventHandler(Char *buf, UInt16 *txtlen, stru_Pref *pref)
{
	FormType **tray_form;
	ListType *lstP;
	
	PointType	inspt_position;	
	UInt32		vol_iterator = vfsIteratorStart;
	UInt16		vol_ref;
	
	DmOpenRef	dbRef=NULL;
	FileRef db_file_ref=NULL;
	
	UInt16 recordIndex;
	Int16 numberOfStrings=0, numberOfAltStrings=0, numberOfSuggestStrings=0;
	MemHandle memHandle=NULL, memStringList;
	Char **itemsPtr;
	Char *record_handle;
	WChar curChar, key;
	EventType event;
	Boolean exit=false;
	UInt16 pos, start;
	UInt16 outLen;
	
	Int16 numItems;
	UInt16 height;

	if ((pref->activeStatus & inJavaMask))//不会从JAVA中取字
		return;

	FldGetSelection (pref->current_field , &start, &pos);
	if (pos>start)//有文字被选中，跳至最后
	{
		FldSetInsertionPoint(pref->current_field, pos);
		FldSetInsPtPosition(pref->current_field, pos);
	}
	else
		pos=FldGetInsPtPosition(pref->current_field);
	outLen=TxtGetPreviousChar(FldGetTextPtr (pref->current_field), pos, &curChar);
	*txtlen = 0;
	
	if (outLen!=2 || !(pref->altChar || pref->suggestChar))//不是汉字或不允许
		return;
		
	recordIndex = GetCharIndex(curChar);//获取字符顺序号

	while (vol_iterator != vfsIteratorStop)//获取储存卡引用,取卡指针
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}

	//打开汉字信息数据库
	dbRef = DmOpenDatabaseByTypeCreator('dict', 'pIME', dmModeReadOnly);
	if (dbRef == NULL && vol_ref > 0)//在内存上没找到数据库，尝试在卡上找
	{
		VFSFileOpen(vol_ref, PIME_CARD_PATH_DICT, vfsModeRead, &db_file_ref);		
	}
	DmGetRecordFromCardAndRAM(dbRef, db_file_ref, recordIndex, &memHandle);	
	if(memHandle == NULL)
		exit = true;
	else//获取列表
	{
		record_handle = (Char *)MemHandleLock(memHandle);
		numberOfAltStrings = *(UInt8 *) (record_handle);
		if(pref->suggestChar)
			numberOfSuggestStrings = *((UInt8 *) (record_handle+1));
		numberOfStrings = (pref->altChar? numberOfAltStrings:0)+numberOfSuggestStrings;
	    if(numberOfStrings)
	    {
		    memStringList = SysFormPointerArrayToStrings(record_handle + 2,
		             numberOfAltStrings + numberOfSuggestStrings);
			itemsPtr = ((Char **)(MemHandleLock(memStringList)));
		}
		else
			exit=true;
		DmReleaseRecordFromCardAndRAM(dbRef, recordIndex, &memHandle);
	}
	//关闭数据库
	DmCloseDatabaseFromCardAndRAM(dbRef, db_file_ref);
	//退出
	if(exit)
		return;
	 
	//新建窗口
	numItems =  numberOfStrings < SUGGEST_LIST_HEIGHT ? numberOfStrings : SUGGEST_LIST_HEIGHT;
	height = numItems * 11 + 4;
	
	InsPtGetLocation(&inspt_position.x, &inspt_position.y); //取光标坐标
	inspt_position.x+=1;
	inspt_position.y-=2;
	(*tray_form) = FrmNewForm(frmAlt, NULL, inspt_position.x > 106 ? 106 : inspt_position.x , inspt_position.y + height > 160 ? 160 - height: inspt_position.y, 54, height, false, NULL, NULL, NULL);	
	LstNewList ((void **)tray_form, lstAlt, 2, 2, 50, height, stdFont, numItems , NULL);
	lstP = (ListType *)FrmGetObjectPtr(*tray_form, FrmGetObjectIndex(*tray_form, lstAlt));
	LstSetDrawFunction(lstP,(ListDrawDataFuncPtr)myDrawFunc);	//自定义画法	
	LstSetListChoices ( lstP, itemsPtr+(pref->altChar? 0:numberOfAltStrings), numberOfStrings );
	//FrmSetFocus(*tray_form, FrmGetObjectIndex(*tray_form, lstAlt));   
    FrmSetActiveForm(*tray_form);
    FrmDrawForm(*tray_form);
    		
	//事件循环
	do
	{
		//获取事件
		EvtGetEvent(&event, evtWaitForever);
		//事件处理
		if (! SysHandleEvent(&event))
		{
			if (event.eType == keyDownEvent) //按键事件
			{				
				KeyTransfer2(&key, &event, pref);//键值转换
				switch (key)
				{
					case 0:
					case vchrHardRockerCenter:
					case vchrRockerCenter://选择
						if(! (event.data.keyDown.modifiers & willSendUpKeyMask))
							EvtEnqueueKey(keySpace, 0, 0);
						break;
					case keySpace:
					case keyReturn:	
					{
						Int16 selection=LstGetSelection(lstP);
						Char *tmp= LstGetSelectionText(lstP, selection);
						(*txtlen) = StrLen(tmp);					
						if (pref->altChar && selection<numberOfAltStrings && (*txtlen)>1)
						{
							if(tmp[(*txtlen)-1]==' ')//字符转换
							{
								(*txtlen)--;//去除空格															
								FldSetSelection(pref->current_field, pos-outLen, pos);//删除当前字
							}
						}
						if((*txtlen)>1 && tmp[(*txtlen)-1]=='-')//去掉分隔符
								(*txtlen)--;//去除分隔符
						StrNCopy(buf, tmp, (*txtlen));
						exit = true;
						break;				
					}
					case vchrPageDown:
					case vchrRockerDown:
					case chrDownArrow://向下
					case hsKeySymbol: //循环
						LstSetSelection(lstP, (LstGetSelection(lstP) + 1< numberOfStrings ) ? (LstGetSelection(lstP) + 1) : 0);
						break;
					case vchrPageUp:
					case vchrRockerUp:
					case chrUpArrow://向上
						LstSetSelection(lstP,LstGetSelection(lstP)? (LstGetSelection(lstP)-1) : (numberOfStrings - 1));
						break;
					case vchrRockerLeft:
					case chrLeftArrow:
						if(LstScrollList(lstP, winUp, SUGGEST_LIST_HEIGHT))
							LstSetSelection(lstP, (LstGetSelection(lstP) < SUGGEST_LIST_HEIGHT) ? 0: LstGetSelection(lstP) - SUGGEST_LIST_HEIGHT);
						else
							exit = true;
						break;
					case vchrRockerRight:
					case chrRightArrow:
						if(LstScrollList(lstP, winDown, SUGGEST_LIST_HEIGHT))
							LstSetSelection(lstP, LstGetSelection(lstP) + SUGGEST_LIST_HEIGHT > numberOfStrings - 1?  numberOfStrings - 1: LstGetSelection(lstP) + SUGGEST_LIST_HEIGHT );
						else
							exit = true;
						break;						
					default:
						exit = true;
				}				
				key=0;//清除键值
			}
			else if(event.eType == lstSelectEvent)
			{
				EvtEnqueueKey(keySpace, 0, 0);			
			}
			else if(event.eType == winExitEvent && event.data.winExit.enterWindow==0)
			{				
				FldSetInsertionPoint(pref->current_field, pos);
				exit = true;
			}
			else
			{
				FrmHandleEvent((*tray_form), &event);
			}
		}
	}while(event.eType != appStopEvent && exit == false);

	
	//释放内存    
    MemHandleUnlock(memStringList);
	MemHandleFree(memStringList);
	//关闭窗口
	FrmEraseForm(*tray_form);
	FrmReturnToForm(0);
}
//
//符号盘
/*static void PuncTrayEventHandler(Char *buf, UInt16 *txtlen, stru_Pref *pref)
{
	UInt16			row = 0;
	UInt16			col = 0;
	UInt16			i;
	Boolean			exit = false;
	WChar			key = 0;
	Char			**punc;
	FormType		*tray_form;
	FieldType		**punc_field;
	EventType		event;
	MemHandle  rscHandle, listHandle;
	Char       *rsc ;

	rscHandle = DmGetResource(strListRscType,PuncTrayList) ;
	if ( rscHandle == NULL )
		return;		
	rsc = MemHandleLock(rscHandle) ;
	listHandle = SysFormPointerArrayToStrings(rsc+3, 7) ;
	punc = MemHandleLock(listHandle) ;
	MemHandleUnlock(rscHandle);
	DmReleaseResource(rscHandle);
				
	//打开窗口
	tray_form = FrmInitForm(frmPunc);
	//分配内存
	punc_field = (FieldType **)MemPtrNew(28);	
	//获取文本框指针
	punc_field[0] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP1));
	punc_field[1] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP2));
	punc_field[2] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP3));
	punc_field[3] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP4));
	punc_field[4] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP5));
	punc_field[5] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP6));
	punc_field[6] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP7));
	//构造文本框内容（标点符号字符串）
	//把标点符号串绑定到文本框
	for (i = 0; i < 7; i ++)
	{
		FldSetTextPtr(punc_field[i], punc[i]);
		//FldDrawField(punc_field[i]);
	}
	//设定被选择的文本
	FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
	//FldDrawField(punc_field[row]);
	FrmSetActiveForm(tray_form);
	FrmDrawForm(tray_form);
	
	//事件循环
	do
	{
		//获取事件
		EvtGetEvent(&event, evtWaitForever);
		//事件处理
		if (! SysHandleEvent(&event))
		{
			if (event.eType == keyDownEvent) //按键事件
			{
				//键值转换
				KeyTransfer2(&key, &event, pref);
				switch (key)
				{
					case 28:
					case vchrRockerLeft: //左移
					{
						//清除当前的选择
						FldSetSelection(punc_field[row], col, col);
						//修改坐标
						if (col > 1)
						{
							col -= 2;
						}
						else
						{
							col = StrLen(punc[row]) - 1;
						}
						col = GetStartPosition(col, punc[row]);
						//设定选择内容
						FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
						FldDrawField(punc_field[row]);
						break;
					}
					case 29:
					case vchrRockerRight: //右移
					{
						//清除当前的选择
						FldSetSelection(punc_field[row], col, col);
						//修改坐标
						col += (GetEndPosition(col, punc[row]) - col);
						if (col == StrLen(punc[row]))
						{
							col = 0;
						}
						col = GetStartPosition(col, punc[row]);
						//设定选择内容
						FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
						FldDrawField(punc_field[row]);
						break;
					}
					case 30:
					case vchrPageUp: //上移
					case 31:
					case vchrPageDown: //下移
					{
						//清除当前的选择
						FldSetSelection(punc_field[row], col, col);
						//修改坐标
						if(key==30 || key==vchrPageUp)
						{
							if (row == 0)
								row = 6;
							else
								row--;
						}
						else
						{						
							if (row == 6)
								row = 0;
							else
								row++;
						}
						col = GetStartPosition(col, punc[row]);
						//设定选择内容
						FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
						FldDrawField(punc_field[row]);
						break;
					}
					case 0x20:
					case vchrRockerCenter: //选择
					{
						//获取选择的内容
						(*txtlen) = GetEndPosition(col, punc[row]) - col;
						StrNCopy(buf, (punc[row] + col), (*txtlen));
						exit = true;
						break;
					}
					case keyBackspace:
					{
						exit = true;
					}
				}
				//清除键值
				key = 0;
			}
			else
			{
				FrmHandleEvent(tray_form, &event);
			}
		}
	}while(event.eType != appStopEvent && exit == false);
	//解除符号串与文本框的绑定，并释放内存
	for (i = 0; i < 7; i ++)
	{
		FldSetTextPtr(punc_field[i], NULL);
		//MemPtrFree(punc[i]);
	}
	//释放内存    
	MemHandleUnlock(listHandle);
	MemHandleFree(listHandle);	
	//MemPtrFree(punc);
	MemPtrFree(punc_field);
	//关闭窗口
	FrmReturnToForm(0);
}*/

//
//符号软键盘, 增强
static void PuncTrayEventHandler(Char *buf, UInt16 *txtlen, stru_Pref *pref)
{
	UInt16			i;
	Boolean			exit = false;
	WChar			key = 0;	
	FormType		*tray_form;		
	ListType 		*listP;
	EventType		event, ep;

	//打开窗口
	tray_form = FrmInitForm(frmPunc);	
	FrmSetActiveForm(tray_form);
	FrmDrawForm(tray_form);

	listP=FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, lstPuncKind));
	LstSetSelection(listP, pref->PuncType);
	CtlSetLabel(FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, triggerPunc)),  LstGetSelectionText (listP, pref->PuncType));
	ep.eType = popSelectEvent;
	ep.data.popSelect.selection = pref->PuncType;
	ep.data.popSelect.controlID=triggerPunc;
	ep.data.popSelect.listID=lstPuncKind;
	EvtAddEventToQueue(&ep);
	(*txtlen)=0;
			
	//事件循环
	do
	{
		//获取事件
		EvtGetEvent(&event, evtWaitForever);
		//事件处理
		if (! SysHandleEvent(&event))
		{
			if (event.eType == keyDownEvent) //按键事件
			{
				//键值转换
				KeyTransfer2(&key, &event, pref);
				switch (key)
				{
					case keyBackspace:
					{
						*txtlen=0;
						exit = true;
						break;
					}
					case hsKeySymbol:
					{
						CtlHitControl(FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, triggerPunc)));
						break;
					}
					default:
					{
						UInt16 newkey;
						if (key==keyReturn || key==vchrRockerCenter || key==keySpace)
							newkey=FrmGetFocus(tray_form)-FrmGetObjectIndex(tray_form, btnPuncA);
						else if(key>=keyComma && key<=keyNine)
							newkey=key-keyComma+26;
						else
							newkey=key-keyA;
						if (newkey>=0 && newkey<40)
						{
							CtlHitControl(FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, btnPuncA+newkey)));
						}
						else
							FrmHandleEvent(tray_form, &event);
					}
				}
				//清除键值
				key = 0;
			}
			else if (event.eType == ctlSelectEvent && event.data.ctlSelect.controlID!=triggerPunc) //按键事件
			{
				StrCopy(buf, CtlGetLabel (event.data.ctlSelect.pControl));
				TreoKBDynamicPunc(buf); //动态命令符号
				*txtlen=StrLen(buf);
				exit=true;
			}
			else if (event.eType == popSelectEvent)//显示键盘
			{
				FrmHandleEvent(tray_form, &event);
				pref->PuncType = event.data.popSelect.selection;
				if(pref->PuncType)//读取资源中的符号列表
				{
					MemHandle  rscHandle, listHandle;
					Char       *rsc ;
					Char		**punc;
					UInt8		punc_num;
					rscHandle = DmGetResource(strListRscType,DefaultPuncList+pref->PuncType) ;
					rsc = MemHandleLock(rscHandle);
					punc_num=*(UInt8 *)(rsc+2);
					listHandle = SysFormPointerArrayToStrings(rsc+3, punc_num) ;
					punc = MemHandleLock(listHandle);
					for(i=0;i<26;i++)	
						CtlSetLabel(FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, btnPuncA+i)), punc[i]);					
					for(i=26;i<40;i++)	
						CtlSetLabel(FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, btnPuncComma+(i-26))), (punc_num==40?punc[i]:""));
					MemHandleUnlock(listHandle);
					MemHandleFree(listHandle);
					MemPtrFree(punc);
					
					MemHandleUnlock(rscHandle);
					DmReleaseResource(rscHandle);
				}
				else//自定义符号
				{
					for(i=0;i<40;i++)
						CtlSetLabel(FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, btnPuncA+i)),i>=26?"":pref->CustomLP[i]);			
				}							
			}
			else
			{
				FrmHandleEvent(tray_form, &event);
			}
		}
	}while(event.eType != appStopEvent && exit == false);
	//关闭窗口
	FrmReturnToForm(0);
}


//--------------------------------------------------------------------------
//码表切换界面
static void MBSwitchEventHandler(stru_Pref *pref, Boolean tempSwitch)
{
	UInt32				vol_iterator = vfsIteratorStart;
	UInt16				vol_ref;
	UInt16				mb_num;
	UInt16				i;
	UInt16				j;
	UInt16				k;
	UInt16				**mb_index;
	Int16				list_selection;
	Char				*full_path;
	Char				**mb_list;
	//Boolean				exit;
	stru_MBList			*mb_list_unit;
	MemHandle			record_handle;
	MemHandle			mb_record_handle;
	FormType			*frmP;
	ListType			*lstP;
	DmOpenRef			dbRef;
	DmOpenRef			dbRef1;
	FileRef				db_file_ref;

	//获取储存卡引用
	//取卡指针
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	//打开码表信息数据库
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	//分配内存
	full_path = (Char *)MemPtrNew(100);
	mb_list = (Char **)MemPtrNew((mb_num << 2));
	mb_index = (UInt16 **)MemPtrNew((mb_num << 2));
	//取已启用的码表数
	j = 0;
	k = mb_num;
	for (i = 0; i < k; i ++)
	{
		record_handle = DmQueryRecord(dbRef, i);
		mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
		if (! mb_list_unit->MBEnabled)
		{
			mb_num --;
		}
		else
		{
			if (mb_list_unit->MBDbType == pref->curMBInfo.db_type) //设定列表选择项
			{
				list_selection = j;
			}
			//分配内存
			mb_list[j] = (Char *)MemPtrNew(32);
			mb_index[j] = (UInt16 *)MemPtrNew(2);
			if (mb_list_unit->inRAM) //内存码表，读取名称
			{
				dbRef1 = DmOpenDatabaseByTypeCreator(mb_list_unit->MBDbType, appFileCreator, dmModeReadOnly);
				mb_record_handle = DmQueryRecord(dbRef1, 0);
				StrCopy(mb_list[j], (Char *)MemHandleLock(mb_record_handle));
				MemHandleUnlock(mb_record_handle);
				DmCloseDatabase(dbRef1);
			}
			else
			{
				if (vol_ref > 0)
				{
					//构造路径
					StrCopy(full_path, PIME_CARD_PATH);
					StrCat(full_path, mb_list_unit->file_name);
					//获取数据库文件索引
					VFSFileOpen(vol_ref, full_path, vfsModeRead, &db_file_ref);
					VFSFileDBGetRecord(db_file_ref, 0, &mb_record_handle, NULL, NULL);
					StrCopy(mb_list[j], (Char *)MemHandleLock(mb_record_handle));
					MemHandleUnlock(mb_record_handle);
					MemHandleFree(mb_record_handle);
					VFSFileClose(db_file_ref);
				}
				else
				{
					StrCopy(mb_list[j], mb_list_unit->file_name);
				}
			}
			//记录码表记录的记录号
			(*mb_index[j]) = i;
			j ++;
		}
		MemHandleUnlock(record_handle);
	}
	//关闭数据库
	DmCloseDatabase(dbRef);

	
	if(tempSwitch) //临时切换
	{
		if(pref->activeStatus & tempMBSwitchMask) //临时状态
		{
			pref->activeStatus &= (~tempMBSwitchMask); //回正常状态
			list_selection--;
			if(list_selection==-1)
				list_selection=mb_num-1;
			ShowStatus(frmMainSwitchMB, mb_list[list_selection], 100);			
		}
		else //正常状态
		{
			pref->activeStatus |= tempMBSwitchMask; //至临时状态
			list_selection++;
			if(list_selection==mb_num)//到最后一个了，切换到第一个
				list_selection = 0;
			ShowStatus(frmTempSwitchMB, mb_list[list_selection], 300);		
		}		
	}	
	else if(pref->AutoMBSwich)//自动切换码表
	{
		list_selection++;
		if(list_selection==mb_num)//到最后一个了，切换到第一个
				list_selection = 0;		
		pref->activeStatus &= (~tempMBSwitchMask); //回复到正常状态
		ShowStatus(frmAutoSwitchMB, mb_list[list_selection], 400);
	}	
	else
	{
		//打开码表切换窗口
		frmP = FrmInitForm(frmSwitchMB);
		lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstMB));
		//设置列表
		LstSetListChoices(lstP, mb_list, mb_num);
		LstSetSelection(lstP, list_selection);		
		FrmSetActiveForm(frmP);
		FrmDrawForm(frmP);					
		list_selection=LstPopupList(lstP);
		FrmEraseForm(frmP);
	}	
	FrmReturnToForm(0);	
	if (list_selection >= 0)
	{
		//获取码表信息
		GetMBInfoFormMBList(&pref->curMBInfo, (*mb_index[list_selection]), false, pref->dync_load);
		GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load);
		PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	}		
	//释放内存
	for (i = 0; i < mb_num; i ++)
	{
		MemPtrFree(mb_list[i]);
		MemPtrFree(mb_index[i]);
	}
	MemPtrFree(mb_list);
	MemPtrFree(mb_index);
	MemPtrFree(full_path);
	
}
//--------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////////////////

//---------------------键盘驱动----------------------------------------------------------------
//清除Grf状态
static void ClearGrfState(stru_Pref *pref)
{
	if (pref->isTreo == 0)
	{
		GrfCleanState();
		//GrfSetState(false,false,false) ;
	}
	else
	{
		HsGrfSetStateExt(false, false, false, false, false, false);
	}
}
//--------------------------------------------------------------------------
//检测Grf指示器是否处于锁定状态
static Boolean GrfLocked(stru_Pref *pref)
{
	Boolean	capsLock = false;
	Boolean	numLock = false;
	Boolean	optLock = false;
	Boolean	autoShifted = false;
	UInt16	tempShift = 0;
	
	if (pref->isTreo)
	{
		HsGrfGetStateExt(&capsLock, &numLock, &optLock, &tempShift, &autoShifted);
	}
	else
	{
		GrfGetState(&capsLock, &numLock, &tempShift, &autoShifted);
	}
	
	if (tempShift == grfTempShiftUpper && (!autoShifted))
	{
		pref->hasShiftMask = true;
		pref->hasOptionMask = false;
	}
	else if (tempShift == hsGrfTempShiftOpt)
	{
		pref->hasShiftMask = false;
		pref->hasOptionMask = true;
	}
	else
	{
		pref->hasShiftMask = false;
		pref->hasOptionMask = false;
	}
	return (capsLock | numLock | optLock);
}
//--------------------------------------------------------------------------
//把结果压入键盘队列（用于Java、DTG中）
static void EnqueueResultToKey(Char *buf, UInt16 buflen)
{
	UInt16 i = 0;
	WChar ch;

	while (i < buflen)
	{
		i += TxtGetNextChar(buf, i, &ch);
		if (/*((UInt16)ch) >= 128*/true)
		{
			EvtEnqueueKey(ch, 0, 0);
		}
	}
}
//--------------------------------------------------------------------------
//颜色转换
UInt16 Make16BitRGBValue (UInt16 r, UInt16 g, UInt16 b)
{
    return (r & 0x1f << 11 | g & 0x3f << 5 | b & 0x1f);
}
//--------------------------------------------------------------------------
//JavaDTG模式画色块
static void DrawPixel(RGBColorType *color,RGBColorType *colorEdge,UInt8 *javaStatusStyleX,UInt8 *javaStatusStyleY,UInt8 javaStyle)
{
	Int x,y;
	RGBColorType	prevRgbP;
	UInt8 width = *javaStatusStyleX;
	UInt8 height = *javaStatusStyleY;
	UInt32        udwWinVersion;
    Boolean       bHiRes;
    
    UInt16        uwPrevCoord;
    MemHandle     bmpH=0;
    RectangleType typRect;
    WinHandle     winH, oldWinH;
    BitmapType    *bitmapP;
    Err           err;
	UInt32		transparentValue = Make16BitRGBValue(0xffff,0xffff,0xff);
	
	// 判断Windows Manager的版本
	FtrGet(sysFtrCreator, sysFtrNumWinVersion, &udwWinVersion);
	if (udwWinVersion >= 4)
	{
			bHiRes = true;
	}
	else
	{
			bHiRes = false;
	}
	
   // 如果不是高分辨率，直接绘制
	if (! bHiRes)
	{
		//矩阵
		if(javaStyle == 2)
		{
			WinSetForeColorRGB (color, &prevRgbP);
			for(x = 0 ; x < width; x++)
				for( y = 0 ; y < height; y++)
				{
					WinDrawPixel ((159-x), (159-y));
				}
			WinSetForeColorRGB (&prevRgbP, NULL);
		}
		//切角
		if(javaStyle == 3)
		{
			WinSetForeColorRGB (color, &prevRgbP);
			for(x = 1 ; x < width-1; x++)
				for( y = x ; y < width-1; y++)
				{
					WinDrawPixel ((159-x), (159-width+1+y));
				}
			WinSetForeColorRGB (&prevRgbP, NULL);
			
			WinSetForeColorRGB (colorEdge, &prevRgbP);
			for(x=0;x < width; x++)
				for(y=0;y<2;y++)
				{
					WinDrawPixel (159, (159-x));
					WinDrawPixel ((159-x), 159);
					WinDrawPixel ((159-x), (159+1-width+x+y));
				}
			WinSetForeColorRGB (&prevRgbP, NULL);
		}
	}
	
	
	// 下面采用高分辨率进行绘制
	// 设置Native坐标系
	uwPrevCoord = WinSetCoordinateSystem(kCoordinatesNative);

	winH = WinCreateOffscreenWindow(320, 320, nativeFormat, &err);
	if (err)
	{
			// 恢复坐标系
			WinSetCoordinateSystem(uwPrevCoord);       
			return;
	}
	// 先在虚拟窗口中绘图，采用低分辨率绘制
	
	bitmapP = WinGetBitmap(winH);
	BmpSetTransparentValue (bitmapP,transparentValue);
	BmpSetDensity(bitmapP, kDensityLow);
	oldWinH = WinSetDrawWindow(winH);

	//矩阵
	if(javaStyle == 2)
	{
		WinSetForeColorRGB (color, &prevRgbP);
		for(x = 0 ; x < width; x++)
			for( y = 0 ; y < height; y++)
			{
				WinDrawPixel ((159-x), (159-y));
			}
		WinSetForeColorRGB (&prevRgbP, NULL);
	}
	//切角
	if(javaStyle == 3)
	{
		WinSetForeColorRGB (color, &prevRgbP);
		for(x = 1 ; x < width-1; x++)
			for( y = x ; y < width-1; y++)
			{
				WinDrawPixel ((159-x), (159-width+1+y));
			}
		WinSetForeColorRGB (&prevRgbP, NULL);
		
		WinSetForeColorRGB (colorEdge, &prevRgbP);
		for(x=0;x < width; x++)
			for(y=0;y<2;y++)
			{
				WinDrawPixel (159, (159-x));
				WinDrawPixel ((159-x), 159);
				WinDrawPixel ((159-x), (159+1-width+x+y));
			}
		WinSetForeColorRGB (&prevRgbP, NULL);
	}
		
	typRect.topLeft.x = 0;
	typRect.topLeft.y = 0;
	typRect.extent.x = 160;
	typRect.extent.y = 160;


	// 复制到原来的窗口，以高分辨率绘制
	BmpSetDensity(bitmapP, kDensityDouble);
	WinSetDrawWindow(oldWinH);
	WinCopyRectangle(winH, 0, &typRect, 160, 160, winPaint);
	WinDeleteWindow(winH,0);
   
	// 恢复坐标系
	WinSetCoordinateSystem(uwPrevCoord);  
}
/*
static void DrawPixel(RGBColorType *color,RGBColorType *colorEdge,UInt8 *javaStatusStyleX,UInt8 *javaStatusStyleY,UInt8 javaStyle)
{
	Int x,y;
	RGBColorType	prevRgbP;
	UInt8 width = *javaStatusStyleX;
	UInt8 height = *javaStatusStyleY;
	
	//矩阵
	if(javaStyle == 2)
	{
		WinSetForeColorRGB (color, &prevRgbP);
		for(x = 0 ; x < width; x++)
			for( y = 0 ; y < height; y++)
			{
				WinDrawPixel ((159-x), (159-y));
			}
		WinSetForeColorRGB (&prevRgbP, NULL);
	}
	//切角
	if(javaStyle == 3)
	{
		WinSetForeColorRGB (color, &prevRgbP);
		for(x = 1 ; x < width-1; x++)
			for( y = x ; y < width-1; y++)
			{
				WinDrawPixel ((159-x), (159-width+1+y));
			}
		WinSetForeColorRGB (&prevRgbP, NULL);
		
		WinSetForeColorRGB (colorEdge, &prevRgbP);
		for(x=0;x < width; x++)
			for(y=0;y<2;y++)
			{
				WinDrawPixel (159, (159-x));
				WinDrawPixel ((159-x), 159);
				WinDrawPixel ((159-x), (159+1-width+x+y));
			}
		WinSetForeColorRGB (&prevRgbP, NULL);
	}

}
*/
static void ErasePixel(UInt8 *javaStatusStyleX,UInt8 *javaStatusStyleY)
{
	Int x,y;
	UInt8 width = *javaStatusStyleX;
	UInt8 height = *javaStatusStyleY;
	for(x = 0 ; x < width; x++)
		for( y = 0 ; y < height; y++)
		{
			WinErasePixel ((159-x), (159-y));
		}
}
//-----------------------------------------------
//标准模式输出
static void Output(WChar curKey, stru_Pref *pref)
{
	UInt8 			operation = 0xFF;
	UInt16			txtlen;
	Char			*buf, *realhead;
	//分配缓存
	buf = (Char *)MemPtrNew(100);
	do
	{
		MemSet(buf, 100, 0x00);
		SetKeyRates(true, pref); //恢复默认按键重复率
		//打开输入框，接收输入，并返回结果的操作码
		operation = PIMEEventHandler(&curKey, buf, pref, FrmGetActiveForm());
		if (operation == pimeCreateWord) //打开手动造词对话框
		{
			//CreateWordEventHandler(buf, bufK, pref);
			SetKeyRates(false, pref); //加快按键重复率
		}
		else
		{
			ClearGrfState(pref);//返回结果					
			SetKeyRates(false, pref); //加快按键重复率
			txtlen = StrLen(buf);
			if (buf[0] == chrSpace)	//英文码表，跳过开头空格，补充到最后			
			{
				realhead = buf + 1;
				buf[txtlen] = chrSpace;
			}
			else
				realhead = buf;
			if ((pref->activeStatus & inJavaMask)) //Java、DTG模式
				EnqueueResultToKey(realhead, txtlen);
			else if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen)//标准模式
				FldInsert(pref->current_field, realhead, txtlen);
		}
	}while (operation == pimeReActive);
	//释放内存
	MemPtrFree(buf);
	if(pref->activeStatus & tempMBSwitchMask)//临时码表，回到正常状态
		MBSwitchEventHandler(pref, true);
}
//------------------------
//清空事件队列
static void ClearEvent(SysNotifyParamType *notifyPtr, EventType *ep)
{
	notifyPtr->handled = true;
	MemSet(ep, sizeof(EventType), 0x00);
	ep->eType = nilEvent;
}
//--------------------------------------------------------------------------
//Treo键盘事件处理
static void TreoKeyboardEventHandler(SysNotifyParamType *notifyPtr, EventType *ep, WChar curKey, UInt16 curKeyCode, UInt16 curModifiers, stru_Pref *pref)
{
	Char			*buf;
	UInt8			operation = 0xFF;
	UInt16			txtlen=0;
	UInt16			cardNo;
	LocalID			dbID;
	RGBColorType	prevRgbP;
	/*Char			*doubleStr1,*doubleStr2,*doubleStr3,*doubleStr4,*doubleStr5,*doubleStr6,*doubleStr7,*doubleStr8,*doubleStr9,*doubleStr10,*doubleStr11,*doubleStr12,*doubleStr13,*doubleStr14,*doubleStr15; 
	
	doubleStr1="《》";
	doubleStr2="（）";
	doubleStr3="“”";
	doubleStr4="‘’";
	doubleStr5="〈〉";
	doubleStr6="〖〗";
	doubleStr7="［］";
	doubleStr8="｛｝";
	doubleStr9="「」";
	doubleStr10="『』";
	doubleStr11="〔〕";
	doubleStr12="()";
	doubleStr13="{}";
	doubleStr14="[]";
	doubleStr15="<>";*/
	
	if ((curModifiers & willSendUpKeyMask)) //按下或长按
	{
		pref->keyDownDetected = true;
		if (! (pref->activeStatus & tempDisabledMask)) //处于中文状态
		{
			if ((curModifiers & autoRepeatKeyMask) && (! pref->longPressHandled)) //长按，且按键未处理
			{
				if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]) || curKey == keyPeriod || (pref->isTreo == isTreo600 && curKeyCode == hsKeySymbol))
				{ //长按，标点符号或数字
					//取对应的中文标点
					buf = TreoKBPuncEventHandler(curKey, curKeyCode, pref, true);
					//返回结果
					txtlen = StrLen(buf);
					if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //未达到文本框尺寸限制
					{
						//清空事件队列
						ClearEvent(notifyPtr, ep);
						//标记本次按键已处理
						pref->isLongPress = true;
						pref->longPressHandled = true;
						if ((pref->activeStatus & inJavaMask)) //Java、DTG模式
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else //标准模式
						{
							FldInsert(pref->current_field, buf, txtlen);
							//if (StrLen(buf) == 4) //双部件标点，把光标移动到标点的中间
							if (StrLen(buf) == 4 && buf[0]==buf[2] && (UInt8)buf[3]-(UInt8)buf[1]==1)
							/*if (StrCompare (buf,doubleStr1)==0 ||
							 StrCompare (buf,doubleStr2)==0 ||
							 StrCompare (buf,doubleStr3)==0 ||
							 StrCompare (buf,doubleStr4)==0 ||
							 StrCompare (buf,doubleStr5)==0 ||
							 StrCompare (buf,doubleStr6)==0 ||
							 StrCompare (buf,doubleStr7)==0 ||
							 StrCompare (buf,doubleStr8)==0 ||
							 StrCompare (buf,doubleStr9)==0 ||
							 StrCompare (buf,doubleStr10)==0 ||
							 StrCompare (buf,doubleStr11)==0)*/
							{
								FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
							}
							/*else if (StrCompare (buf,doubleStr12)==0 ||
							 StrCompare (buf,doubleStr13)==0 ||
							 StrCompare (buf,doubleStr14)==0 ||
							 StrCompare (buf,doubleStr15)==0)*/
							else if (StrLen(buf) == 2  && (UInt8)buf[1]<0x7F && buf[1]-buf[0]==1)
							{
								FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 1);
							}
						}
					}
					//释放内存
					MemPtrFree(buf);
				}
			}
			else if ((pref->hasShiftMask || pref->hasOptionMask))
			{ //是一个Shift、Option组合键消息
				if ((curKey == pref->MBSwitchKey || (curKey == pref->TempMBSwitchKey && !(pref->activeStatus & tempMBSwitchMask))) && pref->LongPressMBSwich && pref->hasOptionMask)
				{ //Opt+，码表切换
					//屏蔽接收键盘消息
					//SysCurAppDatabase(&cardNo, &dbID);
					//SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					ClearEvent(notifyPtr, ep);
					//打开码表切换界面
					MBSwitchEventHandler(pref, curKey != pref->MBSwitchKey);
					//清空事件队列					
					//标记本次按键已处理
					//pref->isLongPress = true;
					//pref->longPressHandled = true;
					//清除Shift、Option按键状态
					ClearGrfState(pref);
					//打开接收键盘消息
					//SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}
				else if (curKey == keyPeriod || curKey == keyBackspace || pref->opt_fullwidth) //英文“.”、中文“……”或“——”
				{
					//取标点符号
					buf = TreoKBPuncEventHandler(curKey, curKeyCode, pref, false);
					txtlen = StrLen(buf);
					if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //未达到文本框尺寸限制
					{
						//清空事件队列
						ClearEvent(notifyPtr, ep);
						//标记本次按键已处理
						pref->isLongPress = true;
						pref->longPressHandled = true;
						//清除Shift、Option按键状态
						ClearGrfState(pref);
						
						if ((pref->activeStatus & inJavaMask)) //Java、DTG模式
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else //标准模式
						{
							FldInsert(pref->current_field, buf, txtlen);
						}
						//释放内存
						MemPtrFree(buf);
					}
				}
				else if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]))
				{ //不属于长按或组合键的情况
					//标记本次按键事件已处理
					pref->longPressHandled = true;
				}
			}					
			else if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]) ||
					curKey == keyPeriod || curKeyCode == pref->IMESwitchKey || curKeyCode == pref->ListKey || curKeyCode == pref->PuncKey || (!pref->LongPressMBSwich  && (curKeyCode == pref->MBSwitchKey || curKeyCode == pref->TempMBSwitchKey)))
			{ //按键事件已经被处理，清除它
				//清空事件队列
				ClearEvent(notifyPtr, ep);
			}

		}
		else if ((curModifiers & autoRepeatKeyMask) && (! pref->longPressHandled)) //英文状态下长按按键
		{
			if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]) || curKey == keyPeriod || (pref->isTreo == isTreo600 && curKeyCode == hsKeySymbol))
			{ //长按，标点符号或数字
				//取对应的英文标点
				buf = (Char *)MemPtrNew(15);
				MemSet(buf, 15, 0x00);
				//TreoKBEnglishPunc(buf, curKey);
				//返回结果
				txtlen = StrLen(buf);
				if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //未达到文本框尺寸限制
				{
					//清空事件队列
					ClearEvent(notifyPtr, ep);
					//标记本次按键已处理
					pref->isLongPress = true;
					pref->longPressHandled = true;
					if ((pref->activeStatus & inJavaMask)) //Java、DTG模式
					{
						EnqueueResultToKey(buf, txtlen);
					}
					else //标准模式
					{
						FldInsert(pref->current_field, buf, txtlen);
					}
				}
				//释放内存
				MemPtrFree(buf);
			}
		}
		else if (curKeyCode == pref->IMESwitchKey)
		{ //输入法状态切换键在英文状态下被按下，清除它并等待该键被抬起
			//清空事件队列
			ClearEvent(notifyPtr, ep);
		}
	}
	else if ((curModifiers & autoRepeatKeyMask)) //按键被松开
	{
		if (! (pref->activeStatus & tempDisabledMask)) //处于中文状态
		{
			if ((! (pref->longPressHandled || pref->isLongPress)) && pref->keyDownDetected) //按键未被处理
			{
				pref->keyDownDetected = false;
				if (curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1] && StrChr(pref->curMBInfo.used_char, curKey) != NULL)
				{ //按键是可以处理的，打开输入框
					
					//清空事件队列
					ClearEvent(notifyPtr, ep);
					//屏蔽接收键盘消息
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					Output(curKey, pref);//标准模式					
					//打开接受键盘消息
					SysNotifyRegister(cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}
				else if (curKey == keyPeriod)
				{ //符号
					buf = TreoKBPuncEventHandler(curKey, curKeyCode, pref, false);
					txtlen = StrLen(buf);
					if (txtlen > 0)
					{
						//清空事件队列
						ClearEvent(notifyPtr, ep);
						if ((pref->activeStatus & inJavaMask)) //Java、DTG模式
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else if ((FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //标准模式
						{
							FldInsert(pref->current_field, buf, txtlen);
						}
						ClearGrfState(pref);
					}
					MemPtrFree(buf);
				}
				else if (curKeyCode == pref->IMESwitchKey)//中英文切换键，切换到英文状态，暂时关闭输入法
				{
					//清空事件队列
					ClearEvent(notifyPtr, ep);
					//标记状态标志
					pref->activeStatus |= tempDisabledMask;
					//记录输入法状态
					pref->last_mode = imeModeEnglish;
					if (pref->init_mode == initRememberFav)
					{
						SetInitModeOfField(pref);
					}
					//恢复光标颜色及按键重复率
					SetKeyRates(true, pref);
					SetCaretColor(true, pref);
					if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask)) //Java、DTG模式，擦除状态文字
					{
						if(pref->javaStatusStyle == Style1)
						{
							SLWinDrawBitmap(NULL, bmpEnIcon, 19,19, false);
							//WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
							//WinDrawChars("英", 2, 150, 148);
							//WinSetForeColorRGB (&prevRgbP, NULL);
						}
						else if(pref->javaStatusStyle == Style2)
						{
							WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
							WinDrawPixel (159, 159);
							WinDrawPixel (158, 159);
							WinDrawPixel (157, 159);
							WinDrawPixel (157, 158);
							WinDrawPixel (157, 157);
							WinDrawPixel (158, 157);
							WinDrawPixel (159, 157);
							WinDrawPixel (157, 156);
							WinDrawPixel (157, 155);
							WinDrawPixel (158, 155);
							WinDrawPixel (159, 155);
							WinSetForeColorRGB (&prevRgbP, NULL);
						}
						else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
						{
							DrawPixel(&pref->englishStatusColor,&pref->englishEdgeColor,&pref->javaStatusStyleX,&pref->javaStatusStyleY,pref->javaStatusStyle);
						}
					}
				}
				else if(curKey == pref->PuncKey || curKey == pref->ListKey)
				{ //汉字信息
					//屏蔽接收键盘消息
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					//清空事件队列
					ClearEvent(notifyPtr, ep);
					buf = (Char *)MemPtrNew(15);
					MemSet(buf, 15, 0x00);
					curKey == pref->PuncKey ? PuncTrayEventHandler(buf, &txtlen, pref):AltEventHandler(buf, &txtlen, pref);
					if(txtlen)//返回结果
					{
						if ((pref->activeStatus & inJavaMask))
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else if ((FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen)
						{
							FldInsert(pref->current_field, buf, txtlen);
							if (((UInt8)(*buf) > 0x7F && StrLen(buf) == 4 && buf[0]==buf[2] && (UInt8)buf[3]-(UInt8)buf[1]==1) ||
								((UInt8)(*buf) <= 0x7F && StrLen(buf) == 2))
							{
								if ((UInt8)(*buf) <= 0x7F) //英文双字符号
								{
									FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 1);
								}
								else //中文符号
								{
									FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
								}
							}							
						}
					}
					MemPtrFree(buf);
					//打开接收键盘消息
					SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}	
				else if ((curKey == pref->MBSwitchKey || (curKey == pref->TempMBSwitchKey && !(pref->activeStatus & tempMBSwitchMask))) && !pref->LongPressMBSwich)//短按，码表切换
				{
					//屏蔽接收键盘消息
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					//清空事件队列
					ClearEvent(notifyPtr, ep);
					//打开码表切换界面
					MBSwitchEventHandler(pref, curKey != pref->MBSwitchKey);
					//打开接收键盘消息
					SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}
			}
			else if (pref->isLongPress || ((! pref->keyDownDetected) && (curModifiers & poweredOnKeyMask)))	//按键是已经被处理了的，或是未检测到按下的键
			{
				pref->isLongPress = false;
				pref->longPressHandled = false;
				//清空事件队列
				ClearEvent(notifyPtr, ep);
			}
			else if (pref->longPressHandled)
			{
				pref->longPressHandled = false;
				notifyPtr->handled = true;
			}
		}
		else if (pref->longPressHandled || ((! pref->keyDownDetected) && (curModifiers & poweredOnKeyMask)))
		{
			pref->isLongPress = false;
			pref->longPressHandled = false;
			//清空事件队列
			ClearEvent(notifyPtr, ep);
		}
		else if (curKeyCode == pref->IMESwitchKey)
		{ //中英文切换键，切换到中文状态，激活输入法
			//清空事件队列
			ClearEvent(notifyPtr, ep);
			//清除状态标记
			pref->activeStatus &= (~tempDisabledMask);
			//记录输入法状态
			pref->last_mode = imeModeChinese;
			if (pref->init_mode == initRememberFav)
			{
				SetInitModeOfField(pref);
			}
			//设置光标颜色、键盘重复率
			SetKeyRates(false, pref);
			SetCaretColor(false, pref);
			if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask))  //Java、DTG模式，或总是显示模式，显示状态文字
			{										
				if(pref->javaStatusStyle == Style1)
				{
					SLWinDrawBitmap(NULL, bmpChIcon, 19,19, false);
					//WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
					//WinDrawChars("中", 2, 150, 148);
					//WinSetForeColorRGB (&prevRgbP, NULL);
				}
				else if(pref->javaStatusStyle == Style2)
				{
					WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
					WinDrawPixel (159, 159);
					WinDrawPixel (158, 159);
					WinDrawPixel (157, 158);
					WinDrawPixel (157, 157);
					WinDrawPixel (157, 156);
					WinDrawPixel (158, 155);
					WinDrawPixel (159, 155);
					WinSetForeColorRGB (&prevRgbP, NULL);
				}
				else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
				{
					DrawPixel(&pref->chineseStatusColor,&pref->chineseEdgeColor,&pref->javaStatusStyleX,&pref->javaStatusStyleY,pref->javaStatusStyle);
				}
			}
		}
	}
}
//--------------------------------------------------------------------------
//外置键盘事件处理
static void ExtKeyboardEventHandler(SysNotifyParamType *notifyPtr, EventType *ep, WChar curKey, UInt16 curKeyCode, UInt16 curModifiers, stru_Pref *pref)
{
	Char			*buf;
	//UInt8			operation;
	UInt16			txtlen=0;
	UInt16			cardNo;
	LocalID			dbID;
	RGBColorType	prevRgbP;
	//Int x,y;

	if (! (pref->activeStatus & tempDisabledMask)) //中文状态
	{
		if (curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1] && StrChr(pref->curMBInfo.used_char, curKey) != NULL)
		{ //按键是可以处理的，打开输入框
			//清空事件队列
			ClearEvent(notifyPtr, ep);
			//屏蔽接收键盘消息
			SysCurAppDatabase(&cardNo, &dbID);
			SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
			Output(curKey, pref);//标准模式	
			//打开接受键盘消息
			SysNotifyRegister(cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
		}
		else if (curKey == pref->IMESwitchKey)
		{ //中英文切换键，切换到英文状态，暂时关闭输入法
			ClearEvent(notifyPtr, ep);
			pref->activeStatus |= tempDisabledMask;
			pref->last_mode = imeModeEnglish;
			if (pref->init_mode == initRememberFav)
			{
				SetInitModeOfField(pref);
			}
			SetKeyRates(true, pref);
			SetCaretColor(true, pref);
			if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask)) 
			{
				if(pref->javaStatusStyle == Style1)
				{
					SLWinDrawBitmap(NULL, bmpEnIcon, 19,19, false);
					//WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
					//WinDrawChars("英", 2, 150, 148);
					//WinSetForeColorRGB (&prevRgbP, NULL);
				}
				else if(pref->javaStatusStyle == Style2)
				{
					WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
					WinDrawPixel (159, 159);
					WinDrawPixel (158, 159);
					WinDrawPixel (157, 159);
					WinDrawPixel (157, 158);
					WinDrawPixel (157, 157);
					WinDrawPixel (158, 157);
					WinDrawPixel (159, 157);
					WinDrawPixel (157, 156);
					WinDrawPixel (157, 155);
					WinDrawPixel (158, 155);
					WinDrawPixel (159, 155);
					WinSetForeColorRGB (&prevRgbP, NULL);
				}
				else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
				{
					DrawPixel(&pref->englishStatusColor,&pref->englishEdgeColor,&pref->javaStatusStyleX,&pref->javaStatusStyleY,pref->javaStatusStyle);
				}
			}
		}
		else if (curKey == pref->KBMBSwitchKey || (curKey == pref->TempMBSwitchKey && !(pref->activeStatus & tempMBSwitchMask)))
		{ //切换码表
			//屏蔽接收键盘消息
			SysCurAppDatabase(&cardNo, &dbID);
			SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
			//清空事件队列
			ClearEvent(notifyPtr, ep);
			//打开码表切换界面
			MBSwitchEventHandler(pref, curKey != pref->KBMBSwitchKey);
			//打开接收键盘消息
			SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
		}
		else if (curKey == pref->PuncKey || curKey == pref->ListKey)
		{ //符号盘或汉字信息
			//屏蔽接收键盘消息
			SysCurAppDatabase(&cardNo, &dbID);
			SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
			//清空事件队列
			ClearEvent(notifyPtr, ep);
			buf = (Char *)MemPtrNew(15);
			MemSet(buf, 15, 0x00);
			//打开符号盘或汉字信息
			(curKey == pref->PuncKey) ? PuncTrayEventHandler(buf, &txtlen, pref) : AltEventHandler(buf, &txtlen, pref);
			if(txtlen)//有结果
			{
				if ((pref->activeStatus & inJavaMask))
				{
					EnqueueResultToKey(buf, txtlen);
				}
				else if ((FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen)
				{
					FldInsert(pref->current_field, buf, txtlen);
					if (curKey == pref->PuncKey) //部分符号光标定位在中间
					{
						if (((UInt8)(*buf) > 0x7F && StrLen(buf) == 4 && buf[0]==buf[2] && (UInt8)buf[3]-(UInt8)buf[1]==1) ||
							((UInt8)(*buf) <= 0x7F && StrLen(buf) == 2))
						{
							if ((UInt8)(*buf) <= 0x7F) //英文双字符号
							{
								FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 1);
							}
							else //中文符号
							{
								FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
							}
						}
					}
				}
			}
			MemPtrFree(buf);
			//打开接收键盘消息
			SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
		}
		else
		{ //标点符号
			buf = ExtKBPuncEventHandler(curKey, curKeyCode, pref, false);
			txtlen = StrLen(buf);
			if (txtlen > 0)
			{
				ClearEvent(notifyPtr, ep);
				if ((pref->activeStatus & inJavaMask))
				{
					EnqueueResultToKey(buf, txtlen);
				}
				else if ((FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen)
				{
					FldInsert(pref->current_field, buf, txtlen);
					if (StrLen(buf) > 2 && (StrCompare(buf, "——") != 0 && StrCompare(buf, "……") != 0))
					{
						FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
					}
				}
			}
			MemPtrFree(buf);
		}
	}
	else if (curKey == pref->IMESwitchKey)
	{ //中英文切换键，切换到中文状态，激活输入法
		ClearEvent(notifyPtr, ep);
		pref->activeStatus &= (~tempDisabledMask);
		pref->last_mode = imeModeChinese;
		if (pref->init_mode == initRememberFav)
		{
			SetInitModeOfField(pref);
		}
		SetKeyRates(false, pref);
		SetCaretColor(false, pref);
		if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask)) 
		{										
			if(pref->javaStatusStyle == Style1)
			{
				SLWinDrawBitmap(NULL, bmpChIcon, 19,19, false);
				//WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
				//WinDrawChars("中", 2, 150, 148);
				//WinSetForeColorRGB (&prevRgbP, NULL);
			}
			else if(pref->javaStatusStyle == Style2)
			{
				WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
				WinDrawPixel (159, 159);
				WinDrawPixel (158, 159);
				WinDrawPixel (157, 158);
				WinDrawPixel (157, 157);
				WinDrawPixel (157, 156);
				WinDrawPixel (158, 155);
				WinDrawPixel (159, 155);
				WinSetForeColorRGB (&prevRgbP, NULL);
			}
			else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
			{
				DrawPixel(&pref->chineseStatusColor,&pref->chineseEdgeColor,&pref->javaStatusStyleX,&pref->javaStatusStyleY,pref->javaStatusStyle);
			}
		}
	}
}
#pragma mark -
//--------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////////////////////

//---------------------启动模块----------------------------------------------------------------
//取激活的文本框
static FieldType *GetActiveField(stru_Pref *pref)
{
	FormType	*curForm;
	UInt16		curObject;
	TableType	*curTable;
	
	curForm = FrmGetActiveForm(); //取当前活动窗体
	if (curForm) //窗体存在
	{
		UInt16 id=1200;
		curObject = FrmGetFocus(curForm); //取获得焦点的对象
		if (curObject != noFocus) //当前对象具备焦点
		{
			if (FrmGetObjectType(curForm, curObject) == frmFieldObj) //普通文本框
			{
				pref->field_in_table = false;
				return FrmGetObjectPtr(curForm, curObject);
			}
			else if (FrmGetObjectType(curForm, curObject) == frmTableObj) //表格中的文本框
			{
				pref->field_in_table = true;
				curTable = FrmGetObjectPtr(curForm, curObject);
				return TblGetCurrentField(curTable);
			}
		}
	}
	
	return NULL;
}
//--------------------------------------------------------------------------
//检测当前窗口是否具备窗体
static Boolean isVaildWindow()
{
	WinHandle		curWin		= WinGetActiveWindow();
	FormType		*curForm	= FrmGetActiveForm();
	
	if (curWin == (WinHandle)curForm && curForm != NULL)
	{
		return true;
	}
	
	return false;
}
//--------------------------------------------------------------------------
//激活输入法
static void ActiveIME(stru_Pref *pref)
{
	pref->Actived = true;
	pref->hasShiftMask = false;
	pref->hasOptionMask = false;
	pref->isLongPress = false;
	pref->longPressHandled = false;
	switch (pref->init_mode)
	{
		case initDefaultChinese: //默认中文
		{
			pref->activeStatus &= (~tempDisabledMask);
			SetKeyRates(false, pref);
			SetCaretColor(false, pref);
			pref->last_mode = imeModeChinese;
			break;
		}
		case initDefaultEnglish: //默认英文
		{
			pref->activeStatus |= tempDisabledMask;
			SetKeyRates(true, pref);
			SetCaretColor(true, pref);
			pref->last_mode = imeModeEnglish;
			break;
		}
		case initKeepLast: //最后状态
		{
			if (pref->last_mode == imeModeChinese)
			{
				pref->activeStatus &= (~tempDisabledMask);
				SetKeyRates(false, pref);
				SetCaretColor(false, pref);
			}
			else
			{
				pref->activeStatus |= tempDisabledMask;
				SetKeyRates(true, pref);
				SetCaretColor(true, pref);
			}
			break;
		}
		case initRememberFav: //记住状态
		{
			pref->last_mode = GetInitModeOfField(pref);
			if (pref->last_mode == imeModeChinese)
			{
				pref->activeStatus &= (~tempDisabledMask);
				SetKeyRates(false, pref);
				SetCaretColor(false, pref);
			}
			else
			{
				pref->activeStatus |= tempDisabledMask;
				SetKeyRates(true, pref);
				SetCaretColor(true, pref);
			}
			break;
		}
	}
	
	pref->activeStatus &= /*(~inJavaMask) & */(~optActiveJavaMask);
}
//--------------------------------------------------------------------------
//启动函数
UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
	SysNotifyParamType	*notifyPtr;
	stru_Pref			*pref;
	UInt16				prefSize;
	UInt32				prefAddress;
	UInt16				cardNo;
	LocalID				dbID;
	EventType			*ep;
	WChar				curKey;
	UInt16				curKeyCode;
	UInt16				curModifiers;
	FieldType			*current_field;
	//Int x,y;
	RGBColorType	prevRgbP;
	
	switch (cmd)
	{
		case sysAppLaunchCmdNotify:
		{
			notifyPtr = (SysNotifyParamType *)cmdPBP;
			switch (notifyPtr->notifyType)
			{
				case sysNotifyEventDequeuedEvent: //事件出列通知
				{
					pref = (stru_Pref *)notifyPtr->userDataP;
					ep = (EventType *)notifyPtr->notifyDetailsP;
					switch (ep->eType)
					{
						case NativeFldEnterEvent: //发生文本框进入
						{
							FieldType	*theField = GetActiveField(pref);
							if (pref->current_field != theField)
							{
								pref->current_field = theField;
								ActiveIME(pref);
							}
							break;
						}
						case NativeKeyDownEvent: //发生按键
						{
							//获取并修正按键
							curKey = CharToLower(ByteSwap16(ep->data.keyDown.chr));
							curKeyCode = ByteSwap16(ep->data.keyDown.keyCode);
							curModifiers = ByteSwap16(ep->data.keyDown.modifiers);
							//获取活动的文本框
							current_field = GetActiveField(pref);
							if (current_field) //有活动的文本框
							{
								pref->current_field = current_field;
								if ((! pref->Actived) || current_field != pref->current_field) //增强模式启用，或文本框改变，激活输入法
								{
									ActiveIME(pref);
								}
								if (isVaildWindow()) //窗体合法，即这是一个From的窗体，而不是一个单纯的window，后者往往是一个menu
								{
									if (((! GrfLocked(pref)) || curKey == pref->IMESwitchKey))
									{ //大写和数字锁定未打开，或按下的是输入法切换键
										switch (pref->KBMode)
										{
											case KBModeTreo:
											{
												TreoKeyboardEventHandler(notifyPtr, ep, curKey, curKeyCode, curModifiers, pref);
												break;
											}
											case KBModeExt:
											case KBModeExtFull:
											{
												ExtKeyboardEventHandler(notifyPtr, ep, curKey, curKeyCode, curModifiers, pref);
												break;
											}
										}
									}
								}
								else if (curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1])
								{ //在单纯的window中检测到能激活输入框的按键，往往这是menu事件，把按键标记为已处理，屏蔽输入框
									pref->keyDownDetected = false;
								}
							}
							else if (pref->DTGSupport) //没有活动的文本框，但DTG、Java支持被启用，转入DTG、Java处理
							{
								if ((pref->isTreo && hasOptionPressed(curModifiers, pref) && curKeyCode == pref->JavaActiveKey) ||
									((! pref->isTreo) && curKey == pref->JavaActiveKey))//DTG模式激活或关闭
								{
									if (curModifiers & willSendUpKeyMask)
									{
										//清除事件
										ClearEvent(notifyPtr, ep);
									}
									else
									{
										if (pref->Actived) //已经激活，关闭
										{
											ClearEvent(notifyPtr, ep);
											pref->Actived = false;
											pref->curWin = NULL;
											pref->current_field = NULL;
											pref->activeStatus &= (~inJavaMask);
											if (pref->activeStatus & tempDisabledMask)
											{
												if(pref->javaStatusStyle == Style1)
												{
													SLWinDrawBitmap(NULL, bmpBlIcon, 19,19, false);
												}
												else if(pref->javaStatusStyle == Style2)
												{
													WinErasePixel (159, 159);
													WinErasePixel (158, 159);
													WinErasePixel (157, 159);
													WinErasePixel (157, 158);
													WinErasePixel (157, 157);
													WinErasePixel (158, 157);
													WinErasePixel (159, 157);
													WinErasePixel (157, 156);
													WinErasePixel (157, 155);
													WinErasePixel (158, 155);
													WinErasePixel (159, 155);
												}
												else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
												{
													ErasePixel(&pref->javaStatusStyleX,&pref->javaStatusStyleY);
												}
											}
											else
											{
												if(pref->javaStatusStyle == Style1)
												{
													SLWinDrawBitmap(NULL, bmpBlIcon, 19,19, false);
												}
												else if(pref->javaStatusStyle == Style2)
												{
													WinErasePixel (159, 159);
													WinErasePixel (158, 159);
													WinErasePixel (157, 158);
													WinErasePixel (157, 157);
													WinErasePixel (157, 156);
													WinErasePixel (158, 155);
													WinErasePixel (159, 155);
												}
												else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
												{
													ErasePixel(&pref->javaStatusStyleX,&pref->javaStatusStyleY);
												}
											}
										}
										else //未激活，打开
										{
											//清除事件消息
											ClearEvent(notifyPtr, ep);
											pref->Actived = true;
											pref->hasShiftMask = false;
											pref->hasOptionMask = false;
											pref->isLongPress = false;
											pref->activeStatus &= (~tempDisabledMask);
											pref->activeStatus |= inJavaMask; //置java状态
											pref->curWin = NULL;
											pref->current_field = NULL;
										}
										ClearGrfState(pref);
									}
								}
								else if ((pref->activeStatus & inJavaMask)) //打开输入框
								{
									switch (pref->KBMode)
									{
										case KBModeTreo:
										{
											TreoKeyboardEventHandler(notifyPtr, ep, curKey, curKeyCode, curModifiers, pref);
											break;
										}
										case KBModeExt:
										{
											ExtKeyboardEventHandler(notifyPtr, ep, curKey, curKeyCode, curModifiers, pref);
											break;
										}
									}
								}
							}
							else if (pref->Actived) //没有活动文本框、DTG未启用，输入法激活，使其休眠
							{
								SetKeyRates(true, pref);
								pref->Actived = false;
								pref->curWin = NULL;
								pref->current_field = NULL;
							}
							break;
						}
						default:
						{
							if (pref->Actived) //输入法已激活
							{
								if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask)) //若处于Java、DTG中，绘制状态图标
								{
									if (isVaildWindow())
									{
										if ((pref->activeStatus & tempDisabledMask)) //英文状态
										{
											if(pref->javaStatusStyle == Style1)
											{
												SLWinDrawBitmap(NULL, bmpEnIcon, 19,19, false);
												//WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
												//WinDrawChars("英", 2, 150, 148);
												//WinSetForeColorRGB (&prevRgbP, NULL);
											}
											else if(pref->javaStatusStyle == Style2)
											{
												WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
												WinDrawPixel (159, 159);
												WinDrawPixel (158, 159);
												WinDrawPixel (157, 159);
												WinDrawPixel (157, 158);
												WinDrawPixel (157, 157);
												WinDrawPixel (158, 157);
												WinDrawPixel (159, 157);
												WinDrawPixel (157, 156);
												WinDrawPixel (157, 155);
												WinDrawPixel (158, 155);
												WinDrawPixel (159, 155);
												WinSetForeColorRGB (&prevRgbP, NULL);
											}
											else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
											{
												DrawPixel(&pref->englishStatusColor,&pref->englishEdgeColor,&pref->javaStatusStyleX,&pref->javaStatusStyleY,pref->javaStatusStyle);
											}
										}
										else //中文状态
										{											
											if(pref->javaStatusStyle == Style1)
											{
												//DrawChIcon();
												SLWinDrawBitmap(NULL, bmpChIcon, 19,19, false);
												//WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
												//WinDrawChars("中", 2, 150, 148);
												//WinSetForeColorRGB (&prevRgbP, NULL);
											}
											else if(pref->javaStatusStyle == Style2)
											{
												WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
												WinDrawPixel (159, 159);
												WinDrawPixel (158, 159);
												WinDrawPixel (157, 158);
												WinDrawPixel (157, 157);
												WinDrawPixel (157, 156);
												WinDrawPixel (158, 155);
												WinDrawPixel (159, 155);
												WinSetForeColorRGB (&prevRgbP, NULL);
											}
											else if(pref->javaStatusStyle == Style3 || pref->javaStatusStyle == Style4)
											{
												DrawPixel(&pref->chineseStatusColor,&pref->chineseEdgeColor,&pref->javaStatusStyleX,&pref->javaStatusStyleY,pref->javaStatusStyle);
											}
										}
									}
								}
								else if (GetActiveField(pref) == NULL) //没有活动的文本框，输入法休眠
								{
									SetKeyRates(true, pref);
									pref->Actived = false;
									pref->curWin = NULL;
									pref->current_field = NULL;
								}
							}
							break;
						}
					}
					break;
				}
				case sysNotifyInsPtEnableEvent: //光标状态通知
				{
					pref = (stru_Pref *)notifyPtr->userDataP;
					if ((! pref->Actived) && (*(Boolean *)notifyPtr->notifyDetailsP)) //光标被激活，启动输入法
					{
						pref->current_field = GetActiveField(pref);
						ActiveIME(pref);
					}
					break;
				}
				case sysNotifyVolumeMountedEvent: //储存卡生效，仅当重启后无法获取码表时才会注册本消息，等待系统装载储存卡
				{
					pref = (stru_Pref *)notifyPtr->userDataP;
					//取消对该消息的注册，确保只运行一次
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyVolumeMountedEvent, sysNotifyNormalPriority);
					//尝试获取码表信息
					if (GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load))
					{ //成功，启动输入法
						MemPtrSetOwner(pref, 0);
						prefAddress = (UInt32)pref;
						FtrSet(appFileCreator, ftrPrefNum, prefAddress);
						MemPtrSetOwner(pref, 0);
						SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
						SysNotifyRegister (cardNo, dbID, sysNotifyInsPtEnableEvent, NULL, sysNotifyNormalPriority, pref);
					}
					else
					{ //失败，关闭输入法
						pref->Enabled = false;
						PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
						MemPtrFree(pref);
					}
					break;
				}
			}
			break;
		}
		case sysAppLaunchCmdSystemReset: //重启自动启动
		{
			prefSize = sizeof(stru_Pref);
			pref = (stru_Pref *)MemPtrNew(prefSize);
			MemSet(pref, prefSize, 0x00);
			if (PrefGetAppPreferences(appFileCreator, appPrefID, pref, &prefSize, true) != noPreferenceFound)
			{
				if (pref->Enabled)
				{
					pref->curMBInfo.key_syncopate = NULL;
					pref->curMBInfo.key_translate = NULL;
					SysCurAppDatabase(&cardNo, &dbID);
					MemPtrSetOwner(pref, 0);
					if (GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load))
					{
						prefAddress = (UInt32)pref;
						FtrSet(appFileCreator, ftrPrefNum, prefAddress);
						SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
						SysNotifyRegister (cardNo, dbID, sysNotifyInsPtEnableEvent, NULL, sysNotifyNormalPriority, pref);
					}
					else
					{
						SysNotifyRegister(cardNo, dbID, sysNotifyVolumeMountedEvent, NULL, sysNotifyNormalPriority, pref);
					}
				}
				else
				{
					MemPtrFree(pref);
				}
			}
			else
			{
				MemPtrFree(pref);
			}
			break;
		}
		case 13:
		case sysAppLaunchCmdNormalLaunch:
		{
			FrmGotoForm(MainForm);
			MainFormEventHandler(false);
			FrmCloseAllForms();
			break;
		}
		case sysAppLaunchCmdDALaunch:
        {
            FrmPopupForm(MainForm);
			MainFormEventHandler(true);			
			break;
        }
	}

	return errNone;
}
#pragma mark ******************* Public ************************
//--------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////////////////
//---------------------公共模块----------------------------------------------------------------
//把按键转换成小写字母
static WChar CharToLower(WChar key)
{
	if (key >= 'A' && key <= 'Z')
	{
		key += 32;
	}
	
	return key;
}
//获取自动切音、码值转换和模糊音列表
static Boolean GetMBDetailInfo(stru_MBInfo *mb_info, Boolean ActivedOnly, Boolean mb_loaded)
{
	UInt16		i;
	UInt16		head_sp;
	UInt16		tail_sp;
	UInt16		vol_ref;
	UInt32		read_size = 0;
	UInt32		vol_iterator = vfsIteratorStart;
	Char		*record;
	Char		*record_save;
	Char		*full_path;
	MemHandle	record_handle;
	DmOpenRef	db_ref;
	FileRef		file_ref = 0;
	
	if (mb_info->db_type > 0)
	{
		//打开码表
		if (mb_loaded || mb_info->inRAM)
		{
			db_ref = DmOpenDatabaseByTypeCreator(mb_info->db_type, 'pIME', dmModeReadOnly);
			if (db_ref != NULL)
			{
				//取信息记录
				record_handle = DmQueryRecord(db_ref, 0);
			}
			else
			{
				return false;
			}
		}
		else
		{
			//取卡指针
			while (vol_iterator != vfsIteratorStop)
			{
				VFSVolumeEnumerate(&vol_ref, &vol_iterator);
			}
			full_path = (Char *)MemPtrNew(100);
			//构造完整路径
			StrCopy(full_path, PIME_CARD_PATH);
			StrCat(full_path, mb_info->file_name);
			VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref);
			MemPtrFree(full_path);
			if (file_ref > 0)
			{
				VFSFileDBGetRecord(file_ref, 0, &record_handle, NULL, NULL);
			}
			else
			{
				return false;
			}
		}
		record = (Char *)MemHandleLock(record_handle);
		//清空模糊音信息
		for (i = 0; i < 11; i ++)
		{
			MemSet(mb_info->blur_head[i].key1, 5, 0x00);
			MemSet(mb_info->blur_head[i].key2, 5, 0x00);
			MemSet(mb_info->blur_tail[i].key1, 5, 0x00);
			MemSet(mb_info->blur_tail[i].key2, 5, 0x00);
			mb_info->blur_head[i].actived = false;
			mb_info->blur_tail[i].actived = false;
		}
		record_save = record; //保存原始记录偏移量
		//取自动切音信息
		if (mb_info->syncopate_offset > 0)
		{
			record += mb_info->syncopate_offset; //偏移到自动切音
			mb_info->key_syncopate = MemPtrNew(mb_info->syncopate_size); //分配内存
			MemMove(mb_info->key_syncopate, record, mb_info->syncopate_size); //取信息
			MemPtrSetOwner(mb_info->key_syncopate, 0);
			record = record_save; //恢复原始偏移量
		}
		//取码值转换信息
		if (mb_info->translate_offset > 0)
		{
			record += mb_info->translate_offset; //偏移到码值转换
			mb_info->key_translate = MemPtrNew(mb_info->translate_size); //分配内存
			MemMove(mb_info->key_translate, record, mb_info->translate_size); //取信息
			MemPtrSetOwner(mb_info->key_translate, 0);
			record = record_save; //恢复原始偏移量
		}
		//取模糊音信息
		record += mb_info->smart_offset; //偏移到模糊音
		head_sp = 0;
		tail_sp = 0;
		while (read_size < mb_info->smart_size) //循环至模糊音结尾
		{
			if (*record == '<') //前模糊音
			{
				record ++; //后移至键1
				read_size ++;
				//检测分隔符
				i = 0;
				while (record[i] != '-' && record[i] != '=')
				{
					i ++;
				}
				//填入码表信息
				if(record[i] == '=' || ((! ActivedOnly) && record[i] == '-')) //应该读取该项
				{
					if (record[i] == '=')
					{
						mb_info->blur_head[head_sp].actived = true;
					}
					StrNCopy(mb_info->blur_head[head_sp].key1, record, i);
					record += i + 1; //后移至键2
					read_size += i + 1;
					i = 0;
					//检测结束符
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					StrNCopy(mb_info->blur_head[head_sp].key2, record, i);
					record += i + 1; //后移至下一个键
					read_size += i + 1;
					head_sp ++;
				}
				else //跳过该项
				{
					record += i + 1; //后移至键2
					read_size += i + 1;
					i = 0;
					//检测结束符
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					record += i + 1; //后移至下一个键
					read_size += i + 1;
				}
			}
			else //后模糊音
			{
				record ++; //后移至键1
				read_size ++;
				//检测分隔符
				i = 0;
				while (record[i] != '-' && record[i] != '=')
				{
					i ++;
				}
				//填入码表信息
				if(record[i] == '=' || ((! ActivedOnly) && record[i] == '-')) //应该读取该项
				{
					if (record[i] == '=')
					{
						mb_info->blur_tail[tail_sp].actived = true;
					}
					StrNCopy(mb_info->blur_tail[tail_sp].key1, record, i);
						record += i + 1; //后移至键2
					read_size += i + 1;
					i = 0;
					//检测结束符
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					StrNCopy(mb_info->blur_tail[tail_sp].key2, record, i);
					record += i + 1; //后移至下一个键
					read_size += i + 1;
					tail_sp ++;
				}
				else //跳过该项
				{
					record += i + 1; //后移至键2
					read_size += i + 1;
					i = 0;
					//检测结束符
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					record += i + 1; //后移至下一个键
					read_size += i + 1;
				}
			}
		}
		//释放记录，关闭码表
		MemHandleUnlock(record_handle);
		if (mb_loaded || mb_info->inRAM)
		{
			DmCloseDatabase(db_ref);
		}
		else
		{
			MemHandleFree(record_handle);
			VFSFileClose(file_ref);
		}
	}
	
	return true;
}
//--------------------------------------------------------------------------
//检查码表是否有修改过，若有，清除修改过的标记并返回真；否则返回假
static Boolean MBModified(stru_MBInfo *mb_info)
{
	DmOpenRef		db_ref;
	UInt16			i;
	UInt16			attr;
	Boolean			modified = false;
	
	//打开数据库
	db_ref = DmOpenDatabaseByTypeCreator(mb_info->db_type, 'pIME', dmModeReadWrite);
	//循环所有记录
	for (i = 0; i < 703; i ++)
	{
		//获取记录的细节信息
		DmRecordInfo(db_ref, i, &attr, NULL, NULL);
		if ((attr & 0x40)) //dirty标志存在，码表被修改过
		{
			modified = true;
			//擦除该标记
			attr = (attr & 0xFFBF);
			DmSetRecordInfo(db_ref, i, &attr, NULL);
		}
	}
	//关闭数据库
	DmCloseDatabase(db_ref);
	
	return modified;
}
//--------------------------------------------------------------------------
//检查要载入的码表在内存中是否存在，若存在，返回真
static Boolean MBExistInRAM(Char *full_path)
{
	UInt16			vol_ref;
	UInt32			vol_iterator = vfsIteratorStart;
	UInt32			dir_iterator = vfsIteratorStart;
	UInt32			db_type;
	Boolean			mb_exist_in_ram = false;
	FileRef			file_ref;
	DmOpenRef		db_ref;
	
	//取卡指针
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref != 0) //卡存在
	{
		if (VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref) == errNone) //打开码表
		{
			//偏移到码表类型
			VFSFileSeek(file_ref, vfsOriginBeginning, 60);
			//读取码表类型
			VFSFileRead (file_ref, 4, &db_type, NULL);
			//关闭码表
			VFSFileClose(file_ref);
			db_ref = DmOpenDatabaseByTypeCreator(db_type, 'pIME', dmModeReadOnly);
			if (DmGetLastErr() == errNone) //码表已经存在
			{
				DmCloseDatabase(db_ref);
				mb_exist_in_ram = true;
			}
		}
	}
	
	return mb_exist_in_ram;
}
//
//
static void ShowStatus(UInt16 rscID, Char *chars, Int32 ms)
{
	FormType *frmP;
	frmP = FrmInitForm(rscID);
	FrmSetActiveForm(frmP);
	FrmDrawForm(frmP);
	if(chars)
		WinDrawChars(chars, StrLen(chars), 50, 4);
	if (ms>0)
		SysTaskDelay(SysTicksPerSecond()*ms/1000);
}
//--------------------------------------------------------------------------
//装载或卸下储存卡上的码表
static void SaveLoadMB(stru_MBInfo *mb_info, UInt8 op, Boolean show_status)
{
	UInt16				vol_ref;
	UInt16				card_no;
	UInt32				vol_iterator = vfsIteratorStart;
	Char				*full_path;
	LocalID				db_id;
	RectangleType		info_rectangle;
	WindowType			*save_win;
	FormType			*frmP;
	DmSearchStateType	stateInfo;
	
	//取卡指针
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref > 0) //卡存在
	{
		switch (op)
		{
			case LOAD: //载入
			{
				//构造完整路径
				full_path = (Char *)MemPtrNew(StrLen(mb_info->file_name) + 27);
				StrCopy(full_path, PIME_CARD_PATH);
				StrCat(full_path, mb_info->file_name);
				if (! MBExistInRAM(full_path)) //码表在内存中不存在
				{
					if (show_status)
					{
						//显示载入提示窗体
						frmP = FrmInitForm(frmLoadMB);
						FrmSetActiveForm(frmP);
						FrmDrawForm(frmP);
					}
					else
					{
						//描绘载入提示
						info_rectangle.topLeft.x = 6;
						info_rectangle.topLeft.y = 18;
						info_rectangle.extent.x = 79;
						info_rectangle.extent.y = 19;
						save_win = WinSaveBits(&info_rectangle, &card_no);
						info_rectangle.topLeft.x = 8;
						info_rectangle.topLeft.y = 20;
						info_rectangle.extent.x = 75;
						info_rectangle.extent.y = 15;
						WinEraseRectangle(&info_rectangle, 2);
						WinDrawRectangleFrame(dialogFrame, &info_rectangle);
						WinDrawChars("载入码表....", 12, 20, 23);
					}
					//载入码表
					VFSImportDatabaseFromFile(vol_ref, full_path, &card_no, &db_id);
					if (show_status)
					{
						//关闭载入提示
						FrmReturnToForm(0);
					}
					else
					{
						//恢复绘图区域
						WinRestoreBits(save_win, 6, 18);
					}
				}
				//释放内存
				MemPtrFree(full_path);
				break;
			}
			case SAVE: //卸下
			{
				if (show_status)
				{
					//显示卸下提示
					frmP = FrmInitForm(frmSaveMB);
					FrmSetActiveForm(frmP);
					FrmDrawForm(frmP);
				}
				else
				{
					//描绘卸载提示
					info_rectangle.topLeft.x = 6;
					info_rectangle.topLeft.y = 18;
					info_rectangle.extent.x = 79;
					info_rectangle.extent.y = 19;
					save_win = WinSaveBits(&info_rectangle, &card_no);
					info_rectangle.topLeft.x = 8;
					info_rectangle.topLeft.y = 20;
					info_rectangle.extent.x = 75;
					info_rectangle.extent.y = 15;
					WinEraseRectangle(&info_rectangle, 2);
					WinDrawRectangleFrame(dialogFrame, &info_rectangle);
					WinDrawChars("保存码表....", 12, 20, 23);
				}
				//构造完整路径
				full_path = (Char *)MemPtrNew(StrLen(mb_info->file_name) + 27);
				StrCopy(full_path, PIME_CARD_PATH);
				StrCat(full_path, mb_info->file_name);
				//取码表信息
				DmGetNextDatabaseByTypeCreator(true, &stateInfo, mb_info->db_type, 'pIME', true, &card_no, &db_id);
				//删除卡上的旧版本码表
				VFSFileDelete(vol_ref, full_path);
				//保存新版本码表
				VFSExportDatabaseToFile(vol_ref, full_path, card_no, db_id);
				//释放内存
				MemPtrFree(full_path);
				if (show_status)
				{
					//关闭卸下提示
					FrmReturnToForm(0);
				}
				else
				{
					//恢复绘图区域
					WinRestoreBits(save_win, 6, 18);
				}
				break;
			}
		}
	}
}
//--------------------------------------------------------------------------
//把载入内存的储存卡码表保存并从内存中移除
static void UnloadMB(UInt16 start, Boolean show_status)
{
	DmOpenRef			dbRef;
	DmSearchStateType	stateInfo;
	MemHandle			record_handle;
	LocalID				db_id;
	UInt16				mb_num;
	UInt16				i;
	UInt16				card_no;
	stru_MBList			*mb_list_unit;
	stru_MBInfo			mb_info;
	
	//打开码表信息数据库
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
	if (! DmGetLastErr())
	{
		mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
		if (mb_num > 0)
		{
			//保存装载到内存中的储存卡码表
			for (i = start; i < mb_num; i ++)
			{
				record_handle = DmQueryRecord(dbRef, i);
				mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
				if (! mb_list_unit->inRAM)
				{
					mb_info.db_type = mb_list_unit->MBDbType;
					if (DmGetNextDatabaseByTypeCreator(true, &stateInfo, mb_info.db_type, 'pIME', true, &card_no, &db_id) == errNone) //存在
					{
						if (MBModified(&mb_info)) //码表修改过，先保存到卡上
						{
							StrCopy(mb_info.file_name, mb_list_unit->file_name);
							//保存到卡上
							SaveLoadMB(&mb_info, SAVE, show_status);
						}
						//从内存中删除
						DmDeleteDatabase(card_no, db_id);
					}
				}
				MemHandleUnlock(record_handle);
			}
		}
		DmCloseDatabase(dbRef);
	}
}
//设置码表信息
static void SetMBInfoByNameType(Char *file_name, UInt32 db_type, Boolean inRAM, stru_MBInfo *mb_info)
{
	Char		*record;
	DmOpenRef	db_ref;
	MemHandle	record_handle;
	
	//打开码表
	db_ref = DmOpenDatabaseByTypeCreator(db_type, 'pIME', dmModeReadWrite);
	//取信息记录
	record_handle = DmGetRecord(db_ref, 0);
	record = (Char *)MemHandleLock(record_handle); 
	//基础信息
	DmWrite(record, 0, mb_info->name, 9); //码表名称
	DmWrite(record, 9, &mb_info->type, 1); //码表类型
	DmWrite(record, 10, &mb_info->index_offset, 4); //索引偏移量
	DmWrite(record, 14, &mb_info->key_length, 1); //码长
	DmWrite(record, 15, mb_info->used_char, 30); //键值范围
	DmWrite(record, 45, &mb_info->wild_char, 1); //万能键
	DmWrite(record, 46, &mb_info->syncopate_offset, 4); //全码元偏移量
	DmWrite(record, 50, &mb_info->syncopate_size, 4); //全码元尺寸
	DmWrite(record, 54, &mb_info->translate_offset, 4); //码值转换偏移量
	DmWrite(record, 58, &mb_info->translate_size, 4); //码值转换尺寸
	DmWrite(record, 62, &mb_info->smart_offset, 4); //模糊码偏移量
	DmWrite(record, 66, &mb_info->smart_size, 4); //模糊码尺寸
	//设置信息
	DmWrite(record, 91, &mb_info->gradually_search, 1); //渐进查找
	DmWrite(record, 92, &mb_info->frequency_adjust, 1); //词频调整
	//关闭码表
	MemHandleUnlock(record_handle);
	DmReleaseRecord(db_ref, 0, true);
	DmCloseDatabase(db_ref);
}
//--------------------------------------------------------------------------
//获取码表信息
static void GetMBInfoByNameType(Char *file_name, UInt32 db_type, Boolean inRAM, stru_MBInfo *mb_info)
{
	Char		*record;
	DmOpenRef	db_ref;
	MemHandle	record_handle;
	UInt16		vol_ref;
	UInt32		vol_iterator = vfsIteratorStart;
	FileRef		file_ref = 0;
	Char		*full_path;
	
	//打开码表
	db_ref = DmOpenDatabaseByTypeCreator(db_type, 'pIME', dmModeReadOnly);
	if (DmGetLastErr() != errNone)
	{
		//取卡指针
		while (vol_iterator != vfsIteratorStop)
		{
			VFSVolumeEnumerate(&vol_ref, &vol_iterator);
		}
		full_path = (Char *)MemPtrNew(100);
		//构造完整路径
		StrCopy(full_path, PIME_CARD_PATH);
		StrCat(full_path, file_name);
		VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref);
		MemPtrFree(full_path);
		VFSFileDBGetRecord(file_ref, 0, &record_handle, NULL, NULL);
	}
	else
	{
		//取信息记录
		record_handle = DmQueryRecord(db_ref, 0);
	}
	record = (Char *)MemHandleLock(record_handle); 
	//基础信息
	MemMove(mb_info->name, record, 9); //码表名称
	record += 9;
	mb_info->type = *(UInt8 *)record; //码表类型
	record ++;
	mb_info->index_offset = *(UInt32 *)record; //索引偏移量
	record += 4;
	mb_info->key_length = *(UInt8 *)record; //码长
	record ++;
	MemMove(mb_info->used_char, record, 30); //键值范围
	record += 30;
	mb_info->wild_char = *(Char *)record; //万能键
	record ++;
	mb_info->syncopate_offset = *(UInt32 *)record; //全码元偏移量
	record += 4;
	mb_info->syncopate_size = *(UInt32 *)record; //全码元尺寸
	record += 4;
	mb_info->translate_offset = *(UInt32 *)record; //码值转换偏移量
	record += 4;
	mb_info->translate_size = *(UInt32 *)record; //码值转换尺寸
	record += 4;
	mb_info->smart_offset = *(UInt32 *)record; //模糊码偏移量
	record += 4;
	mb_info->smart_size = *(UInt32 *)record; //模糊码尺寸
	//设置信息
	record += 25;
	mb_info->gradually_search = *(Boolean *)record; //渐进查找
	record ++;
	mb_info->frequency_adjust = *(Boolean *)record; //词频调整
	//关闭码表
	MemHandleUnlock(record_handle);
	if (db_ref != NULL)
	{
		DmCloseDatabase(db_ref);
	}
	else
	{
		MemHandleFree(record_handle);
		VFSFileClose(file_ref);
	}
}
//--------------------------------------------------------------------------
//通过码表索引设置码表的信息
static void SetMBInfoFormMBList(stru_MBInfo *mb_info, UInt16 mb_index)
{
	DmOpenRef	dbRef;
	stru_MBList	mb_list_unit;
	stru_MBList *record;
	UInt16		mb_num;
	MemHandle	record_handle;
	
	//打开码表信息数据库
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	if (mb_num > 0)
	{
		//取对应的记录
		record_handle = DmGetRecord(dbRef, mb_index);
		record = (stru_MBList *)MemHandleLock(record_handle);
		//构造码表信息库记录信息
		mb_list_unit.MBEnabled = mb_info->enabled;
		mb_list_unit.MBDbType = mb_info->db_type;
		mb_list_unit.inRAM = mb_info->inRAM;
		StrCopy(mb_list_unit.file_name, mb_info->file_name);
		DmWrite(record, 0, &mb_list_unit, sizeof(stru_MBList));
		//释放记录
		MemHandleUnlock(record_handle);
		DmReleaseRecord(dbRef, mb_index, true);
		//设置当前码表的信息
		SetMBInfoByNameType(mb_info->file_name, mb_info->db_type, mb_info->inRAM, mb_info);
	}
	DmCloseDatabase(dbRef);
}
//--------------------------------------------------------------------------*/
//通过码表索引获取码表的信息，如果码表在卡上，则载入内存，并把原来从卡上载入的码表保存至储存卡
static void GetMBInfoFormMBList(stru_MBInfo *mb_info, UInt16 mb_index, Boolean show_status, Boolean need_load_mb)
{
	DmOpenRef			dbRef;
	stru_MBList			*mb_list_unit;
	UInt16				mb_num;
	MemHandle			record_handle;
	
	//打开码表信息数据库
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	if (mb_num > 0)
	{
		//取对应的记录
		record_handle = DmQueryRecord(dbRef, mb_index);
		mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
		//不是同一个码表，需要进行装卸操作
		if (mb_info->db_type != mb_list_unit->MBDbType)
		{
			//保存装载到内存中的储存卡码表
			UnloadMB(unloadAll, show_status);
			//释放内存
			if (mb_info->syncopate_offset > 0 && mb_info->key_syncopate != NULL)
			{
				MemPtrFree(mb_info->key_syncopate);
				mb_info->key_syncopate = NULL;
			}
			if (mb_info->translate_offset > 0 && mb_info->key_translate != NULL)
			{
				MemPtrFree(mb_info->key_translate);
				mb_info->key_translate = NULL;
			}
			StrCopy(mb_info->file_name, mb_list_unit->file_name);
			mb_info->db_type = mb_list_unit->MBDbType;
			mb_info->inRAM = mb_list_unit->inRAM;
			mb_info->enabled = mb_list_unit->MBEnabled;
			if ((! mb_info->inRAM) && need_load_mb) //在卡上，载入内存
			{
				SaveLoadMB(mb_info, LOAD, show_status);
			}
			//获取当前码表的信息
			GetMBInfoByNameType(mb_info->file_name, mb_info->db_type, mb_info->inRAM, mb_info);
		}
		//释放记录
		MemHandleUnlock(record_handle);
	}
	else
	{
		if (mb_info->syncopate_offset > 0)
		{
			MemPtrFree(mb_info->key_syncopate);
			mb_info->key_syncopate = NULL;
		}
		if (mb_info->translate_offset > 0)
		{
			MemPtrFree(mb_info->key_translate);
			mb_info->key_translate = NULL;
		}
		MemSet(mb_info, sizeof(stru_MBInfo), 0x00);
	}
	DmCloseDatabase(dbRef);
}
//--------------------------------------------------------------------------
//通过给定的码表信息库的记录号，返回该记录中记录的码表是否已经启用
static Boolean MBEnabled(UInt16 mb_index)
{
	DmOpenRef		dbRef;
	stru_MBList		*mb_list_unit;
	MemHandle		record_handle;
	Boolean			mb_enabled;
	
	//打开码表信息数据库
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
	//取对应的记录
	record_handle = DmQueryRecord(dbRef, mb_index);
	mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
	mb_enabled = mb_list_unit->MBEnabled;
	//释放记录
	MemHandleUnlock(record_handle);
	//关闭数据库
	DmCloseDatabase(dbRef);
	
	return mb_enabled;
}
#pragma mark -
//--------------------------------------------------------------------------
//检测option键是否被按下
static Boolean hasOptionPressed(UInt16 modifiers, stru_Pref *pref)
{
	Boolean capsLock		= false;
	Boolean	numLock			= false;
	Boolean optLock			= false;
	Boolean autoShifted		= false;
	Boolean	optionPressed	= false;
	UInt16	tempShift		= 0;
	
	if ((modifiers & optionKeyMask)) //带有option掩码
	{
		optionPressed = true;
	}
	else
	{
		if (pref->isTreo) //HS系列状态函数
		{
			HsGrfGetStateExt(&capsLock, &numLock, &optLock, &tempShift, &autoShifted);
		}
		else //标准状态函数
		{
			GrfGetState(&capsLock, &numLock, &tempShift, &autoShifted);
		}
		
		if (tempShift == hsGrfTempShiftOpt)
		{
			optionPressed = true;
		}
	}
	
	return (optionPressed | optLock);
}
//--------------------------------------------------------------------------
//设置或恢复光标颜色
static void SetCaretColor(Boolean set_default, stru_Pref *pref)
{
	RGBColorType	cnCaretColor;
	
	if (set_default)
	{
		cnCaretColor = pref->defaultCaretColor;
		UIColorSetTableEntry(UIFieldCaret, &cnCaretColor);
	}
	else
	{
		cnCaretColor = pref->caretColor;
		UIColorSetTableEntry(UIFieldCaret, &cnCaretColor);
	}
}
//--------------------------------------------------------------------------
//设置或恢复按键延迟
static void SetKeyRates(Boolean reset, stru_Pref *pref)
{
	UInt16 initDelayP;
	UInt16 periodP;
	UInt16 doubleTapDelayP;
	Boolean queueAheadP;
	
	if (! reset) //设置成快速
	{
		KeyRates(false, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
		initDelayP = (SysTicksPerSecond() >> 2);
		KeyRates(true, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
	}
	else //恢复成默认速度
	{
		KeyRates(false, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
		initDelayP = pref->defaultKeyRate;
		KeyRates(true, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
	}
}
//--------------------------------------------------------------------------
//保存当前文本框状态
static void SetInitModeOfField(stru_Pref *pref)
{
	MemHandle		record_handle;
	DmOpenRef		db_ref;
	
	if ((! (pref->activeStatus & inJavaMask)) && pref->current_field != NULL) //状态合法，可以保存
	{
		db_ref = DmOpenDatabaseByTypeCreator('init', appFileCreator, dmModeReadWrite);
		record_handle = DmGetRecord(db_ref, pref->init_mode_record);
		DmWrite(MemHandleLock(record_handle), 6, &pref->last_mode, 1);
		MemHandleUnlock(record_handle);
		DmReleaseRecord(db_ref, pref->init_mode_record, true);
		DmCloseDatabase(db_ref);
	}
}
//--------------------------------------------------------------------------
//获取记录中的文本框状态
static UInt8 GetInitModeOfField(stru_Pref *pref)
{
	DmOpenRef		db_ref;
	FormType		*form;
	stru_InitInfo	init_info;
	stru_InitInfo	*record;
	MemHandle		record_handle;
	UInt16			record_count;
	UInt16			i;
	Boolean			not_found = true;
	
	init_info.mode = imeModeChinese;
	
	if (pref->current_field != NULL) //文本框指针合法
	{
		form = FrmGetActiveForm(); //获取当前窗体
		if (form != NULL) //窗体指针合法
		{
			//获取当前信息
			init_info.form_id = FrmGetFormId(form); //窗体ID
			init_info.object_count = FrmGetNumberOfObjects(form); //窗体的控件数
			if (pref->field_in_table) //表格中的文本框，无法取ID，取表格的ID
			{
				i = FrmGetFocus(form);
				if (FrmGetObjectType(form, i) == frmTableObj) //确实是表格
				{
					init_info.field_id = FrmGetObjectId(form, i); //表格ID
				}
				else //不知道是什么，退出……
				{
					return imeModeChinese;
				}
			}
			else
			{
				init_info.field_id = FrmGetObjectId(form, FrmGetObjectIndexFromPtr(form, pref->current_field)); //文本框ID
			}
			//打开数据库，进行比较
			db_ref = DmOpenDatabaseByTypeCreator('init', appFileCreator, dmModeReadWrite);
			record_count = DmNumRecords(db_ref);
			i = 0;
			while (i < record_count && not_found)
			{
				record_handle = DmQueryRecord(db_ref, i);
				record = (stru_InitInfo *)MemHandleLock(record_handle);
				if (MemCmp(&init_info, record, 6) == 0) //找到结果
				{
					init_info.mode = record->mode;
					pref->init_mode_record = i;
					not_found = false;
				}
				MemHandleUnlock(record_handle);
				i ++;
			}
			//若没有找到匹配的信息，把新信息保存到数据库
			if (not_found)
			{
				i = dmMaxRecordIndex;
				record_handle = DmNewRecord(db_ref, &i, stru_InitInfo_length);
				record = (stru_InitInfo *)MemHandleLock(record_handle);
				DmWrite(record, 0, &init_info, stru_InitInfo_length);
				MemHandleUnlock(record_handle);
				DmReleaseRecord(db_ref, i, true);
				pref->init_mode_record = i;
			}
			//关闭数据库
			DmCloseDatabase(db_ref);
		}
	}
	
	return init_info.mode;
}
//--------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////////////////

//---------------------设置模块----------------------------------------------------------------
//更新模糊音列表
static void UpdateBlurList(ListType *lstP, stru_MBInfo *mb_info, Char ***blur_list, UInt16 *blur_num)
{
	UInt16		i;
	UInt16		j;
	Int16		lst_selection;
	
	//保存列表状态
	lst_selection = LstGetSelection(lstP);
	//清除旧的列表
	if ((*blur_num) > 0)
	{
		for (i = 0; i < (*blur_num); i ++)
		{
			MemPtrFree((*blur_list)[i]);
		}
		MemPtrFree((*blur_list));
		(*blur_num) = 0;
	}
	//获取模糊音的总数
	i = 0;
	while (mb_info->blur_head[i].key1[0] != '\0')
	{
		i ++;
		(*blur_num) ++;
	}
	i = 0;
	while (mb_info->blur_tail[i].key1[0] != '\0')
	{
		i ++;
		(*blur_num) ++;
	}
	//构造新列表
	(*blur_list) = (Char **)MemPtrNew(((*blur_num) << 2));
	i = 0;
	//前模糊音
	j = 0;
	while (mb_info->blur_head[j].key1[0] != '\0')
	{
		(*blur_list)[i] = (Char *)MemPtrNew(StrLen(mb_info->blur_head[j].key1) + StrLen(mb_info->blur_head[j].key2) + 4);
		StrCopy((*blur_list)[i], mb_info->blur_head[j].key1);
		StrCat((*blur_list)[i], "=");
		StrCat((*blur_list)[i], mb_info->blur_head[j].key2);
		if (mb_info->blur_head[j].actived)
		{
			StrCat((*blur_list)[i], "*");
		}
		i ++;
		j ++;
	}
	//后模糊音
	j = 0;
	while (mb_info->blur_tail[j].key1[0] != '\0')
	{
		(*blur_list)[i] = (Char *)MemPtrNew(StrLen(mb_info->blur_tail[j].key1) + StrLen(mb_info->blur_tail[j].key2) + 4);
		StrCopy((*blur_list)[i], mb_info->blur_tail[j].key1);
		StrCat((*blur_list)[i], "=");
		StrCat((*blur_list)[i], mb_info->blur_tail[j].key2);
		if (mb_info->blur_tail[j].actived)
		{
			StrCat((*blur_list)[i], "*");
		}
		i ++;
		j ++;
	}
	//更新列表
	LstSetListChoices(lstP, (*blur_list), (*blur_num));
	LstDrawList(lstP);
	LstSetSelection(lstP, lst_selection);
}
//--------------------------------------------------------------------------
//启停模糊音
static void SwitchBlurActiveStatus(stru_MBInfo *mb_info, UInt16 blur_num, UInt16 blur_index)
{
	UInt16		i = 0;
	UInt16		j = 0;
	UInt32		write_offset;
	Boolean		matched = false;
	Char		*record;
	MemHandle	record_handle;
	DmOpenRef	db_ref;
	
	//查找需要修正的模糊音
	while (mb_info->blur_head[i].key1[0] != '\0')
	{
		if (j == blur_index)
		{
			mb_info->blur_head[i].actived = ! mb_info->blur_head[i].actived;
			matched = true;
			break;
		}
		else
		{
			j ++;
		}
		i ++;
	}
	if (! matched)
	{
		i = 0;
		while (mb_info->blur_tail[i].key1[0] != '\0')
		{
			if (j == blur_index)
			{
				mb_info->blur_tail[i].actived = ! mb_info->blur_tail[i].actived;
				break;
			}
			else
			{
				j ++;
			}
			i ++;
		}
	}
	//保存模糊音
	//打开码表
	db_ref = DmOpenDatabaseByTypeCreator(mb_info->db_type, 'pIME', dmModeReadWrite);
	//获取记录
	record_handle = DmGetRecord(db_ref, 0);
	record = (Char *)MemHandleLock(record_handle);
	//写入模糊音
	write_offset = mb_info->smart_offset;
	//前模糊音
	i = 0;
	while (mb_info->blur_head[i].key1[0] != '\0')
	{
		DmWrite(record, write_offset, "<", 1);
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_head[i].key1, StrLen(mb_info->blur_head[i].key1)); //键1
		write_offset += StrLen(mb_info->blur_head[i].key1);
		if (mb_info->blur_head[i].actived) //启停符号
		{
			DmWrite(record, write_offset, "=", 1);
		}
		else
		{
			DmWrite(record, write_offset, "-", 1);
		}
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_head[i].key2, StrLen(mb_info->blur_head[i].key2)); //键2
		write_offset += StrLen(mb_info->blur_head[i].key2);
		if (write_offset + 1 < mb_info->smart_size) //分隔或结束
		{
			DmWrite(record, write_offset, "\'", 1);
		}
		write_offset ++;
		i ++;
	}
	//后模糊音
	i = 0;
	while (mb_info->blur_tail[i].key1[0] != '\0')
	{
		DmWrite(record, write_offset, ">", 1);
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_tail[i].key1, StrLen(mb_info->blur_tail[i].key1)); //键1
		write_offset += StrLen(mb_info->blur_tail[i].key1);
		if (mb_info->blur_tail[i].actived) //启停符号
		{
			DmWrite(record, write_offset, "=", 1);
		}
		else
		{
			DmWrite(record, write_offset, "-", 1);
		}
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_tail[i].key2, StrLen(mb_info->blur_tail[i].key2)); //键2
		write_offset += StrLen(mb_info->blur_tail[i].key2);
		if (write_offset + 1 < mb_info->smart_size) //分隔或结束
		{
			DmWrite(record, write_offset, "\'", 1);
		}
		write_offset ++;
		i ++;
	}
	//释放记录
	MemHandleUnlock(record_handle);
	DmReleaseRecord(db_ref, 0, true);
	//关闭码表
	DmCloseDatabase(db_ref);
}
//--------------------------------------------------------------------------
//设置模糊音
static void SetBlurEventHandler(stru_Pref *pref, UInt16 mb_index)
{
	UInt16			blur_num = 0;
	UInt16			i;
	Char			**blur_list = NULL;
	Boolean			exit = false;
	stru_MBInfo		mb_info;
	EventType		event;
	FormType		*frmP;
	ListType		*lstP;
	
	//打开模糊音设置窗体
	frmP = FrmInitForm(frmSetBlur);
	FrmDrawForm(frmP);
	FrmSetActiveForm(frmP);
	//获取码表信息
	MemSet(&mb_info, sizeof(stru_MBInfo), 0x00);
	GetMBInfoFormMBList(&mb_info, mb_index, true, true);
	//获取模糊音信息
	GetMBDetailInfo(&mb_info, false, true);
	//获取模糊音列表指针
	lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstBlur));
	//更新模糊音列表
	LstSetSelection(lstP, noListSelection);
	UpdateBlurList(lstP, &mb_info, &blur_list, &blur_num);
	
	//事件循环
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		
		if (! SysHandleEvent(&event))
		{
			switch (event.eType)
			{
				case ctlSelectEvent:
				{
					if (event.data.ctlSelect.controlID == btnBOK) //退出
					{
						exit = true;
					}
					break;
				}
				case lstSelectEvent:
				{
					if (event.data.lstSelect.selection >= 0)
					{
						SwitchBlurActiveStatus(&mb_info, blur_num, (UInt16)event.data.lstSelect.selection);
						UpdateBlurList(lstP, &mb_info, &blur_list, &blur_num);
					}
					break;
				}
				default:
				{
					FrmHandleEvent(frmP, &event);
					break;
				}
			}
		}
	}while(event.eType != appStopEvent && (! exit));
	
	//释放内存
	if (mb_info.syncopate_offset > 0)
	{
		MemPtrFree(mb_info.key_syncopate);
	}
	if (mb_info.translate_offset > 0)
	{
		MemPtrFree(mb_info.key_translate);
	}
	if (blur_num > 0)
	{
		for (i = 0; i < blur_num; i ++)
		{
			MemPtrFree(blur_list[i]);
		}
		MemPtrFree(blur_list);
	}
	//返回
	FrmReturnToForm(0);
}
//--------------------------------------------------------------------------
//按键设置
static WChar CustomKey(UInt8 kb_mode, stru_Pref *settingP)
{
	FormType	*frmP;
	EventType	event;
	WChar		key		= chrNull;
	
	FrmPopupForm(frmSetKeyTips);
	
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		
		if (kb_mode == KBModeTreo)
		{
			if (event.eType == keyUpEvent)
			{
				if (event.data.keyDown.keyCode != 0)
				{
					key = (WChar)event.data.keyDown.keyCode;
				}
				else
				{
					key = (WChar)event.data.keyDown.chr;
				}
			}
			else if (event.eType == keyDownEvent && (! (event.data.keyDown.modifiers & 0x0008)))
			{
				key = event.data.keyDown.chr;
			}
		}
		else if ((kb_mode == KBModeExt || kb_mode == KBModeExtFull) && event.eType == keyDownEvent)
		{
			key = event.data.keyDown.chr;
		}
		
		if (key == chrNull && (event.eType != keyUpEvent && event.eType != keyDownEvent))
		{
			if (! SysHandleEvent(&event))
			{
				if (event.eType == frmOpenEvent && event.data.frmOpen.formID == frmSetKeyTips)
				{
					frmP = FrmInitForm(frmSetKeyTips);
					FrmSetActiveForm(frmP);
					FrmDrawForm(frmP);
				}
				else
				{
					FrmDispatchEvent(&event);
				}
			}
		}
	}while (key == chrNull && event.eType != appStopEvent);
	
	//退出窗体
	FrmReturnToForm(0);
	
	return CharToLower(key);
}
//--------------------------------------------------------------------------
//高级设置界面
static void AdvanceSettingEventHandler(stru_Pref *pref)
{
	EventType		event;
	EventType		ep;
	FormType		*frmP;
	UInt16			i;
	UInt16			cardNo;
	LocalID			dbID;
	Boolean			exit = false;
	ListType		*lstP;
	ControlType		*triP;
	Int16			PrioritySelection;
	
	FrmPopupForm(frmAdvSetting);
	
	//事件循环
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		//if (! SysHandleEvent(&event))
		//{
			switch (event.eType)
			{
				case frmLoadEvent:
				{
					//打开高级设置窗体
					frmP = FrmInitForm(frmAdvSetting);
			
					break;
				}
				case frmOpenEvent:
				{
					FrmDrawForm(frmP);
					FrmSetActiveForm(frmP);
					
					lstP = FrmGetObjectPtr(frmP,FrmGetObjectIndex(frmP,lstNotifyPriority));//优先级选项列表指针
					triP = FrmGetObjectPtr(frmP,FrmGetObjectIndex(frmP,triggerNotifyPriority));//优先级选项列表激活器指针
					
					//显示优先级列表
					if ( pref->NotifyPriority == -128 ) PrioritySelection = 0;
					else if ( pref->NotifyPriority == -96 ) PrioritySelection = 1;
					else if ( pref->NotifyPriority == -64 ) PrioritySelection = 2;
					else if ( pref->NotifyPriority == -32 ) PrioritySelection = 3;
					else PrioritySelection = 4;
					
					LstSetSelection(lstP,PrioritySelection);
					CtlSetLabel(triP,LstGetSelectionText(lstP,LstGetSelection(lstP)));
					
					//键盘驱动模式
					if (pref->KBMode == KBModeTreo)
					{
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeTreo), 1);
						FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetMBSwitchKey));
						FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetSyncopateKey));
						FrmShowObject(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich));
						FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetKBMBSwitchKey));
						//FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetPuncKey));
					}
					else
					{
						if (pref->KBMode == KBModeExt || pref->KBMode == KBModeExtFull)
						{
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeExt), 1);
						}
						else
						{
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeXplore), 1);
						}
						FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetMBSwitchKey));
						FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetSyncopateKey));
						FrmHideObject(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich));
						FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetKBMBSwitchKey));
						//FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetPuncKey));
					}
					//第二组选字键是否启用
					for (i = 0; i < 5; i ++)
					{
						if (pref->Selector2[i] != 0)
						{
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cb2ndSelector), 1);
						}
					}
					
					//Java支持是否启用
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbEnableJava), pref->DTGSupport);
					
					//自动切换码表是否启用
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbAutoMBSwich), pref->AutoMBSwich);
					
					//码表切换键是长按还是短按
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich), pref->LongPressMBSwich);
					
					//显示GSI指示器
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbShowGsiButton), pref->showGsi);
					
					//显示翻页按钮
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbChoiceButton), pref->choice_button);
					
					//显示菜单按钮
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbMenuButton), pref->menu_button);
					
					//固定输入框
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbShowOnBottom), (! pref->shouldShowfloatBar));
					
					break;
				}
				case frmUpdateEvent:
				{
					FrmDrawForm(frmP);
					break;
				}
				case ctlSelectEvent:
				{
					switch (event.data.ctlSelect.controlID)
					{
						case btnRestore: //恢复默认设置
						{
							if (pref->KBMode == KBModeXplore)
							{
								pref->keyRange[0] = keyZero;
								pref->keyRange[1] = keyNine;
							}
							else
							{
								pref->keyRange[0] = keyA;
								pref->keyRange[1] = keyZ;
							}
							//默认按键
							pref->Selector[0] = 0x0020; pref->Selector[1] = keyZero; pref->Selector[2] = hsKeySymbol;
							pref->Selector[3] = keyLeftShift; pref->Selector[4] = keyRightShift;
							//默认按键2
							pref->Selector2[0] = 0; pref->Selector2[1] = 0; pref->Selector2[2] = 0;
							pref->Selector2[3] = 0; pref->Selector2[4] = 0;
							//按键模式
							pref->KBMode = KBModeTreo;
							{
								MemSet(&ep, sizeof(EventType), 0x00);
								ep.eType = ctlSelectEvent;
								ep.data.ctlSelect.controlID = pbtnKBModeTreo;
								//ep.data.lstSelect.pList = lstP;
								//ep.data.lstSelect.selection = 0;
								EvtAddEventToQueue(&ep);
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeTreo), 1);
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeExt), 0);
							}
							//FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeTreo), 1);
							//FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnKBModeExt), 0);
							pref->NotifyPriority = 0;
							//特殊按键
							pref->IMESwitchKey = keyHard1;
							pref->JavaActiveKey = keySpace;
							pref->ListKey = hsKeySymbol;
							pref->KBMBSwitchKey = 0;
							pref->MBSwitchKey = 0;
							pref->TempMBSwitchKey = 0;
							pref->PuncKey = 0;
							pref->SyncopateKey = keyPeriod;
							pref->MenuKey = keyMenu;
							//设置输入法状态
							pref->shouldShowfloatBar = true;
							pref->DTGSupport = false;
							pref->choice_button = false;
							pref->menu_button = false;
							pref->AutoMBSwich = false;
							pref->LongPressMBSwich = true;
							pref->showGsi = true;
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbEnableJava), pref->DTGSupport);
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbAutoMBSwich), pref->AutoMBSwich);
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich), pref->LongPressMBSwich);
							break;
						}
						case pbtnKBModeTreo: //Treo键盘模式
						{
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetMBSwitchKey));
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetSyncopateKey));
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich));
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetKBMBSwitchKey));
							//FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetPuncKey));
							pref->KBMode = KBModeTreo;
							pref->keyRange[0] = keyA;
							pref->keyRange[1] = keyZ;
							FrmDrawForm(frmP);
							break;
						}
						case pbtnKBModeXplore: //权智键盘模式
						{
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetMBSwitchKey));
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetSyncopateKey));
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich));
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetKBMBSwitchKey));
							//FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetPuncKey));
							pref->KBMode = KBModeXplore;
							pref->keyRange[0] = keyZero;
							pref->keyRange[1] = keyNine;
							FrmDrawForm(frmP);
							break;
						}
						case pbtnKBModeExt: //外置键盘模式
						{
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetMBSwitchKey));
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetSyncopateKey));
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich));
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetKBMBSwitchKey));
							//FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetPuncKey));
							//pref->KBMode = KBModeExt;
							pref->keyRange[0] = keyA;
							pref->keyRange[1] = keyZ;
							if (LstPopupList((ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstExtMode))) == 1)
							{
								pref->KBMode = KBModeExtFull;
							}
							else
							{
								pref->KBMode = KBModeExt;
							}
							FrmDrawForm(frmP);
							break;
						}
						case btnSetKBMBSwitchKey: //外置键盘模式-码表切换键
						{
							pref->KBMBSwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetPuncKey: //外置键盘模式-符号盘键
						{
							pref->PuncKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetListKey:
						{
							pref->ListKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}						
						case cbEnableJava: //启停Java、DTG支持
						{
							pref->DTGSupport = ! pref->DTGSupport;
							if (pref->DTGSupport) //启用
							{
								if (pref->Enabled)
								{
									SysCurAppDatabase(&cardNo, &dbID);
									SysNotifyRegister (cardNo, dbID, sysNotifyVirtualCharHandlingEvent, NULL, sysNotifyNormalPriority, pref);
								}
							}
							else //关闭
							{
								if (pref->Enabled)
								{
									pref->activeStatus &= (~inJavaMask);
									SysCurAppDatabase(&cardNo, &dbID);
									SysNotifyUnregister(cardNo, dbID, sysNotifyVirtualCharHandlingEvent, sysNotifyNormalPriority);
								}
							}
							break;
						}
						case cbAutoMBSwich: //启停自动切换码表
						{
							pref->AutoMBSwich = ! pref->AutoMBSwich;
							break;
						}
						case cbLongPressMBSwich: //启停自动切换码表
						{
							pref->LongPressMBSwich = ! pref->LongPressMBSwich;
							break;
						}
						case btnSetSwitchKey: //设置输入法启停键
						{
							pref->IMESwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetMenuKey: //设置菜单键
						{
							pref->MenuKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetMBSwitchKey: //设置输入法码表切换键
						{
							pref->MBSwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetTempMBSwitchKey: //设置输入法码表临时切换键
						{
							pref->TempMBSwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}						
						case btnSetSyncopateKey: //设置切音键
						{
							pref->SyncopateKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetJavaKey: //设置Java启停、切换键
						{
							pref->JavaActiveKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetKey1: //第一组选字键
						case btnSetKey2:
						case btnSetKey3:
						case btnSetKey4:
						case btnSetKey5:
						{
							if (event.data.ctlSelect.controlID == btnSetKey1)
							{
								i = 0;
							}
							else if (event.data.ctlSelect.controlID == btnSetKey2)
							{
								i = 1;
							}
							else if (event.data.ctlSelect.controlID == btnSetKey3)
							{
								i = 2;
							}
							else if (event.data.ctlSelect.controlID == btnSetKey4)
							{
								i = 3;
							}
							else if (event.data.ctlSelect.controlID == btnSetKey5)
							{
								i = 4;
							}
							pref->Selector[i] = CustomKey(pref->KBMode, pref);
							break;
						}
						case cbShowGsiButton: //显示GSI指示器
						{
							pref->showGsi = ! pref->showGsi;
							break;
						}
						case cbShowOnBottom: //固定输入框
						{
							pref->shouldShowfloatBar = ! pref->shouldShowfloatBar;
							break;
						}
						case cbChoiceButton: //启停翻页按钮
						{
							pref->choice_button = ! pref->choice_button;
							break;
						}
						case cbMenuButton: //启停翻页按钮
						{
							pref->menu_button = ! pref->menu_button;
							break;
						}
						case btnExitAdvForm: //退出高级设置
						{
							exit = true;
							break;
						}
						case cb2ndSelector: //启用第二组按键
						{
							if (FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cb2ndSelector)) == 0)
							{
								for (i = 0; i < 5; i ++)
								{
									pref->Selector2[i] = 0;
								}
							}
							break;
						}
						case btnSetKey21: //第二组选字键
						case btnSetKey22:
						case btnSetKey23:
						case btnSetKey24:
						case btnSetKey25:
						{
							if (FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cb2ndSelector)) == 1)
							{
								if (event.data.ctlSelect.controlID == btnSetKey21)
								{
									i = 0;
								}
								else if (event.data.ctlSelect.controlID == btnSetKey22)
								{
									i = 1;
								}
								else if (event.data.ctlSelect.controlID == btnSetKey23)
								{
									i = 2;
								}
								else if (event.data.ctlSelect.controlID == btnSetKey24)
								{
									i = 3;
								}
								else if (event.data.ctlSelect.controlID == btnSetKey25)
								{
									i = 4;
								}
								pref->Selector2[i] = CustomKey(pref->KBMode, pref);
							}
							break;
						}
						case triggerNotifyPriority:
						{
							LstPopupList(lstP);
							PrioritySelection = LstGetSelection (lstP);
							if ( PrioritySelection == 0 ) pref->NotifyPriority = -128;
							else if ( PrioritySelection == 1 ) pref->NotifyPriority = -96;
							else if ( PrioritySelection == 2 ) pref->NotifyPriority = -64;
							else if ( PrioritySelection == 3 ) pref->NotifyPriority = -32;
							else pref->NotifyPriority = 0;
							CtlSetLabel(triP,LstGetSelectionText(lstP,LstGetSelection(lstP)));
							break;
						}
					}
					FrmUpdateForm(frmAdvSetting, 0);
					break;
				}
				default:
				{
					if (! SysHandleEvent(&event))
					{
						FrmDispatchEvent(&event);
					}
					break;
				}
			}
			//FrmHandleEvent(frmP, &event);
		//}
	}while (! (event.eType == appStopEvent || exit));
	
	//保存设置
	PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	//返回
	FrmReturnToForm(0);
}
#pragma mark -
//--------------------------------------------------------------------------
//获取卡上的码表列表
static UInt16 GetMBListOnVFS(stru_MBList ***mb_list_vfs)
{
	UInt16			mb_num = 0;
	UInt16			vol_ref;
	UInt16			i;
	UInt32			vol_iterator = vfsIteratorStart;
	UInt32			dir_iterator = vfsIteratorStart;
	Boolean			dir_exist = false;
	Char			*full_path;
	Char			*file_ext_name;
	FileRef			dir_ref;
	FileRef			file_ref;
	FileInfoType	file_info;
	
	//取卡指针
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref != 0) //卡存在
	{
		if (VFSFileOpen(vol_ref, PIME_CARD_PATH, vfsModeReadWrite, &dir_ref) == errNone) //打开 /PALM/Programs/PocketIME
		{
			file_info.nameP = (Char *)MemPtrNew(32);
			dir_iterator = vfsIteratorStart;
			//遍历文件夹
			while (dir_iterator != vfsIteratorStop)
			{
				if (VFSDirEntryEnumerate(dir_ref, &dir_iterator, &file_info) == errNone)
				{
					file_ext_name = StrChr(file_info.nameP, (WChar)'.'); //取扩展名
					if (file_ext_name != NULL) //扩展名存在
					{
						if (StrNCaselessCompare(file_ext_name, ".pdb", 4) == 0) //是.pdb，认为其是一个数据库
						{
							mb_num ++; //码表计数器+1
						}
					}
				}
			}
			//创建文件列表
			(*mb_list_vfs) = (stru_MBList **)MemPtrNew((mb_num << 2));
			i = 0;
			dir_iterator = vfsIteratorStart;
			while (dir_iterator != vfsIteratorStop)
			{
				if (VFSDirEntryEnumerate(dir_ref, &dir_iterator, &file_info) == errNone)
				{
					file_ext_name = StrChr(file_info.nameP, (WChar)'.'); //取扩展名
					if (file_ext_name != NULL) //扩展名存在
					{
						if (StrNCaselessCompare(file_ext_name, ".pdb", 4) == 0) //是.pdb，认为其是一个数据库
						{
							//分配列表单元的内存
							(*mb_list_vfs)[i] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList));
							MemSet((*mb_list_vfs)[i], sizeof(stru_MBList), 0x00);
							//码表名称
							StrCopy((*mb_list_vfs)[i]->file_name, file_info.nameP);
							i ++;
						}
					}
				}
			}
			VFSFileClose(dir_ref);
			MemPtrFree(file_info.nameP);
			//获取码表类型
			full_path = (Char *)MemPtrNew(10240);
			for (i = 0; i < mb_num; i ++)
			{
				//构造完整路径
				StrCopy(full_path, PIME_CARD_PATH);
				StrCat(full_path, (*mb_list_vfs)[i]->file_name);
				//打开码表
				VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref);
				//读取PDB类型
				VFSFileSeek(file_ref, vfsOriginBeginning, 60);
				VFSFileRead(file_ref, 4, &(*mb_list_vfs)[i]->MBDbType, NULL);
				//关闭码表
				VFSFileClose(file_ref);
			}
			MemPtrFree(full_path);
		}
		else // /PALM/Programs/PocketIME 不存在，创建它
		{
			VFSDirCreate(vol_ref, "/PALM/Programs/PocketIME");
		}
	}
	
	return mb_num;
}
//--
//判断是否为码表类型
static Boolean IsMBType(UInt32 type)
{
	return !(type == sysFileTApplication || type == sysFileTPanel || type == sysResTAppGData || type == 'init' || type == 'dict' || type == 'DAcc');
}
//--------------------------------------------------------------------------
//获取内存中的码表列表
static UInt16 GetMBListInRAM(stru_MBList ***mb_list)
{
	UInt16				mb_num;
	UInt16				db_count = 0;
	UInt16				i;
	UInt16				j;
	MemHandle			db_list_handle;
	SysDBListItemType	*db_list;
	
	if (SysCreateDataBaseList(0, appFileCreator, &db_count, &db_list_handle, false)) //取CreatorID='pIME'的全部数据库
	{
		if (db_count > 0)
		{
			db_list = (SysDBListItemType *)MemHandleLock(db_list_handle);
			//去除type='appl'、'panl'和'data'的计数
			mb_num = db_count;
			for (i = 0; i < db_count; i ++)
			{
				if (!IsMBType(db_list[i].type))
				{
					mb_num --;
				}
			}
			//创建码表列表
			(*mb_list) = (stru_MBList **)MemPtrNew((mb_num << 2));
			j = 0;
			for (i = 0; i < db_count; i ++)
			{
				if (IsMBType(db_list[i].type))
				{
					(*mb_list)[j] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList)); //分配码表列表单元的内存
					MemSet((*mb_list)[j], sizeof(stru_MBList), 0x00);
					//码表类型
					(*mb_list)[j]->MBDbType = db_list[i].type;
					//码表在内存
					(*mb_list)[j]->inRAM = true;
					//码表名称
					StrCopy((*mb_list)[j]->file_name, db_list[i].name);
					j ++;
				}
			}
			//释放内存
			MemHandleUnlock(db_list_handle);
			MemHandleFree(db_list_handle);
		}
	}
	
	return mb_num;
}
//--------------------------------------------------------------------------
//更新码表信息库并生成码表列表
static UInt16 UpdateMBListDB(char ***mb_list)
{
	UInt16		mb_num_ram;
	UInt16		mb_num_vfs;
	UInt16		mb_num;
	UInt16		i;
	UInt16		j;
	UInt16		k;
	Boolean		mb_exist;
	MemHandle	record_handle;
	stru_MBList	*mb_unit;
	stru_MBList	**mb_list_ram;
	stru_MBList	**mb_list_vfs;
	stru_MBList **mb_list_db;
	DmOpenRef	dbRef;
	
	//打开码表信息库
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	if (DmGetLastErr()) //数据库不存在，新建
	{
		DmCreateDatabase(0, "PIME_MBList", appFileCreator, 'data', false);
		dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	}
	//获取位于内存的码表列表
	mb_num_ram = GetMBListInRAM(&mb_list_ram);
	//获取位于储存卡上的码表列表
	mb_num_vfs = GetMBListOnVFS(&mb_list_vfs);
	//获取码表信息库的码表列表（不包含不存在的码表）
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	//分配内存
	(*mb_list) = (char **)MemPtrNew(80); //列表
	mb_list_db = (stru_MBList **)MemPtrNew(80); //码表
	if (mb_num > 0)
	{
		k = 0;
		for (i = 0; i < mb_num; i ++)
		{
			//取一条记录
			record_handle = DmQueryRecord(dbRef, i);
			mb_unit = (stru_MBList *)MemHandleLock(record_handle);
			mb_exist = false;
			if (mb_unit->inRAM) //在内存中的
			{
				//比较内存码表列表
				for (j = 0; j < mb_num_ram; j ++)
				{
					if (mb_unit->MBDbType == mb_list_ram[j]->MBDbType) //存在
					{
						MemSet(mb_list_ram[j]->file_name, 32, 0x00); //标记本项在码表信息库中存在
						mb_exist = true;
						break;
					}
				}
			}
			else //在储存卡上的
			{
				//比较储存卡码表列表
				for (j = 0; j < mb_num_vfs; j ++)
				{
					if (StrCompare(mb_unit->file_name, mb_list_vfs[j]->file_name) == 0)
					{
						MemSet(mb_list_vfs[j]->file_name, 32, 0x00); //标记本项在码表信息库中存在
						mb_exist = true;
						break;
					}
				}
			}
			if (mb_exist) //码表存在
			{
				UInt16 len=StrLen(mb_unit->file_name);
				if(mb_unit->file_name[len-4]=='.')
				{
					(*mb_list)[k] = (char *)MemPtrNew(len - 2);	
					StrNCopy((*mb_list)[k], mb_unit->file_name, len-4);
					(*mb_list)[k][len-4]=0x1a;//卡上码表
					(*mb_list)[k][len-3]=0;
				}
				else
				{
					(*mb_list)[k] = (char *)MemPtrNew(len + 1);					
					StrCopy((*mb_list)[k]+(mb_exist-1), mb_unit->file_name);
				}
				mb_list_db[k] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList));
				MemMove(mb_list_db[k], mb_unit, sizeof(stru_MBList));
				k ++;
			}
			MemHandleUnlock(record_handle);
		}
		//删除码表信息库中的列表
		for (i = 0; i < mb_num; i ++)
		{
			DmRemoveRecord(dbRef, 0);
		}
		//调整码表信息库的列表数
		mb_num = k;
	}
	//添加内存中的新码表
	for (i = 0; i < mb_num_ram; i ++)
	{
		if (mb_list_ram[i]->file_name[0] != '\0')
		{
			mb_num ++;
			(*mb_list)[mb_num - 1] = (Char *)MemPtrNew(StrLen(mb_list_ram[i]->file_name) + 1);
			StrCopy((*mb_list)[mb_num - 1], mb_list_ram[i]->file_name);
			mb_list_db[mb_num - 1] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList));
			MemMove(mb_list_db[mb_num - 1], mb_list_ram[i], sizeof(stru_MBList));
		}
	}
	//添加储存卡中的新码表
	for (i = 0; i < mb_num_vfs; i ++)
	{
		if (mb_list_vfs[i]->file_name[0] != '\0')
		{
			UInt16 len;
			mb_num ++;
			len=StrLen(mb_list_vfs[i]->file_name);
			(*mb_list)[mb_num - 1] = (Char *)MemPtrNew(len - 2);
			StrNCopy((*mb_list)[mb_num - 1], mb_list_vfs[i]->file_name, len-4);
			(*mb_list)[mb_num - 1][len-4]=0x1a;//卡上码表
			(*mb_list)[mb_num - 1][len-3]=0;
			mb_list_db[mb_num - 1] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList));
			MemMove(mb_list_db[mb_num - 1], mb_list_vfs[i], sizeof(stru_MBList));
		}
	}
	MemPtrResize((*mb_list), (mb_num << 2));
	//写入码表信息库
	for (i = 0; i < mb_num; i ++)
	{
		j = dmMaxRecordIndex;
		record_handle = DmNewRecord(dbRef, &j, sizeof(stru_MBList));
		mb_unit = (stru_MBList *)MemHandleLock(record_handle);
		DmWrite(mb_unit, 0, mb_list_db[i], sizeof(stru_MBList));
		MemHandleUnlock(record_handle);
		DmReleaseRecord(dbRef, j, true);
		MemPtrFree(mb_list_db[i]);
	}
	MemPtrFree(mb_list_db);
	//释放内存
	for (i = 0; i < mb_num_ram; i ++)
	{
		MemPtrFree(mb_list_ram[i]);
	}
	for (i = 0; i < mb_num_vfs; i ++)
	{
		MemPtrFree(mb_list_vfs[i]);
	}
	if (mb_num_ram > 0)
	{
		MemPtrFree(mb_list_ram);
	}
	if (mb_num_vfs > 0)
	{
		MemPtrFree(mb_list_vfs);
	}
	//关闭数据库
	DmCloseDatabase(dbRef);
	
	return mb_num;
}
//--------------------------------------------------------------------------
//移动码表信息库的码表记录
static void MoveMBRecordInMBListDB(UInt16 record_index, ListType *lstP, Char ***mb_list, UInt8 direction)
{
	DmOpenRef	db_ref;
	MemHandle	record_handle;
	stru_MBList	*record;
	UInt16		mb_num;
	UInt16		i;
	UInt16		obj_record_index;
	Int16		list_selection;
	
	//打开码表信息库
	db_ref = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	//获取码表信息库的码表数
	mb_num = DmNumRecordsInCategory(db_ref, dmAllCategories);
	if (mb_num > 1)
	{
		//保存列表选定值
		list_selection = LstGetSelection(lstP);
		//清除当前列表
		for (i = 0; i < mb_num; i ++)
		{
			MemPtrFree((*mb_list)[i]);
		}
		//获取移动方向
		if (direction == UP) //上移
		{
			if (record_index > 0)
			{
				obj_record_index = record_index - 1;
				list_selection --;
			}
			else
			{
				obj_record_index = 0;
			}
		}
		else //下移
		{
			if (record_index < mb_num - 1)
			{
				obj_record_index = record_index + 2;
				list_selection ++;
			}
			else
			{
				obj_record_index = mb_num;
			}
		}
		//移动记录
		DmMoveRecord(db_ref, record_index, obj_record_index);
		//重新获取列表
		for (i = 0; i < mb_num; i ++)
		{
			UInt16 len;
			record_handle = DmQueryRecord(db_ref, i);
			record = (stru_MBList *)MemHandleLock(record_handle);
			len=StrLen(record->file_name);
			if(record->file_name[len-4]=='.')
			{
				(*mb_list)[i] = (Char *)MemPtrNew(len - 2);
				StrNCopy((*mb_list)[i], record->file_name, len-4);
				(*mb_list)[i][len-4]=0x1a;//卡上码表
				(*mb_list)[i][len-3]=0;
			}
			else
			{
				(*mb_list)[i] = (Char *)MemPtrNew(len + 1);
				StrCopy((*mb_list)[i], record->file_name);
			}
			MemHandleUnlock(record_handle);
		}
		//绑定到列表
		LstSetListChoices(lstP,(*mb_list), mb_num);
		LstDrawList(lstP);
		//设定被选择的列
		LstSetSelection(lstP, list_selection);
	}
	//关闭码表信息库
	DmCloseDatabase(db_ref);
}
//--------------------------------------------------------------------------
//设置输入法启动状态
static void SetInitModeTrigger(Int16 mode, stru_Pref *pref)
{
	FormType	*frmP;
	ListType	*lstP;
	ControlType	*triP;
	DmOpenRef	db_ref;
	
	frmP = FrmGetActiveForm();
	lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstInitMode));
	triP = (ControlType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, ptriInitMode));
	if (mode >= 0)
	{
		CtlSetLabel(triP, LstGetSelectionText(lstP, mode));
		pref->init_mode = (UInt8)mode;
		if (pref->init_mode == initRememberFav)
		{
			//打开初始模式信息库
			db_ref = DmOpenDatabaseByTypeCreator('init', appFileCreator, dmModeReadWrite);
			if (DmGetLastErr()) //数据库不存在，新建
			{
				DmCreateDatabase(0, "PIME_INIT", appFileCreator, 'init', false);
				db_ref = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
			}
			DmCloseDatabase(db_ref);
		}
	}
	else
	{
		CtlSetLabel(triP, LstGetSelectionText(lstP, (Int16)pref->init_mode));
	}
}
//--------------------------------------------------------------------------
//删除码表
static void DeleteMB(UInt16 mb)
{
	UInt16				card_no;
	LocalID				db_id;
	MemHandle			record_handle;
	stru_MBList			*record;
	DmOpenRef			db_ref;
	DmSearchStateType	state_info;
	UInt16				vol_ref;
	UInt32				vol_iterator = vfsIteratorStart;
	Char				*full_path;
	
	if (FrmAlert(alertConfirmDelete) == 0)
	{
		db_ref = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
		record_handle = DmQueryRecord(db_ref, mb);
		record = (stru_MBList *)MemHandleLock(record_handle);
		if (record->inRAM)
		{
			DmGetNextDatabaseByTypeCreator(true, &state_info, record->MBDbType, appFileCreator, true, &card_no, &db_id);
			DmDeleteDatabase(card_no, db_id);
		}
		else
		{
			full_path = (Char *)MemPtrNew(100);
			StrCopy(full_path, PIME_CARD_PATH);
			StrCat(full_path, record->file_name);
			//取卡指针
			while (vol_iterator != vfsIteratorStop)
			{
				VFSVolumeEnumerate(&vol_ref, &vol_iterator);
			}
			VFSFileDelete(vol_ref, full_path);
			MemPtrFree(full_path);
		}
		MemHandleUnlock(record_handle);
		DmCloseDatabase(db_ref);
	}
}
#pragma mark -
//--------------------------------------------------------------------------
//绘制色块样本
static void PaintCurrentColor(stru_Pref *pref)
{
	RectangleType		rectangle;
	RGBColorType		default_rgb_color;
	RGBColorType		backRGBColor;
	
	rectangle.topLeft.x = 67;
	rectangle.topLeft.y = 20;
	rectangle.extent.x = 6;
	rectangle.extent.y = 10;
	//光标颜色
	WinSetForeColorRGB(&pref->caretColor, &default_rgb_color);
	WinSetBackColorRGB(&default_rgb_color, &backRGBColor);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//边框颜色
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->frameColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//编码前景
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->codeForeColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//中文指示色
	if ( pref->javaStatusStyle != 0)
	{
		WinSetForeColorRGB(&pref->chineseStatusColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//编码背景
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->codeBackColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//英文指示色
	if ( pref->javaStatusStyle != 0)
	{
		WinSetForeColorRGB(&pref->englishStatusColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//结果前景
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultForeColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//结果背景
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultBackColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//中文切角边缘色
	if (pref->javaStatusStyle == 3)
	{
		WinSetForeColorRGB(&pref->chineseEdgeColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//结果高亮前景
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultHighlightForeColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//英文切角边缘色
	if (pref->javaStatusStyle == 3)
	{
		WinSetForeColorRGB(&pref->englishEdgeColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//结果高亮背景
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultHighlightBackColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//恢复前景色
	WinSetForeColorRGB(&default_rgb_color, NULL);
	WinSetBackColorRGB(&backRGBColor, NULL);
}
//--------------------------------------------------------------------------
//画文本框
static Err SetFieldTextFromStr (FieldPtr field, Char *s, Boolean redraw)
{
        MemHandle h;
       
        h = FldGetTextHandle(field);
        if(h)
        {
                Err err;
                FldSetTextHandle(field, NULL);
                err = MemHandleResize(h, StrLen(s)+1);
                if(err!=errNone)
                {
                        FldSetTextHandle(field, h);
                        return err;
                }
        } else {
                h = MemHandleNew(StrLen(s)+1);
                if(!h)
                        return memErrNotEnoughSpace;
        }
       
        StrCopy((Char *)MemHandleLock(h), s);
        MemHandleUnlock(h);
       
        FldSetTextHandle(field, h);
        if(redraw)
                FldDrawField(field);
        return errNone;
}
//--------------------------------------------------------------------------
//重置默认符号
static void ResetSign(stru_Pref *pref)
{
	/*Char lp_str[26][16]={"&", "#", "8", "4", "1", "5", "6",\
						 "￥", "@", "！", "：", "‘’", "，", "？",\
						 "“”", "?d", "/",      "2", "～", "3",\
						 "《》", "9", "+",      "7", "（）", "*"};*/
	MemHandle  rscHandle;	
	rscHandle = DmGetResource(strListRscType,DefaultPuncList) ;
	if ( rscHandle )
	{
		UInt i;
		MemHandle listHandle ;
		Char       *rsc, **list ;
		rsc = MemHandleLock(rscHandle) ;
		listHandle = SysFormPointerArrayToStrings(rsc+3, 30) ;
		list = MemHandleLock(listHandle) ;
		MemHandleUnlock(rscHandle);
		for(i=0;i<26;i++)
			StrCopy(pref->CustomLP[i], *(list++));
		StrCopy(pref->CustomLPPeriod,*(list++));
		StrCopy(pref->CustomLPOptBackspace, *(list++));	
		StrCopy(pref->CustomLPShiftBackspace, *(list++));	
		StrCopy(pref->CustomLPShiftPeriod, *(list++));
		//释放内存    
		MemHandleUnlock(listHandle);
		MemHandleFree(listHandle);		
	}
	DmReleaseResource(rscHandle);	
}
//--------------------------------------------------------------------------
//自定义符号
static void CustomLongPressEventHandler(stru_Pref *pref)
{
	EventType			event;
	FormType			*form;
	Boolean				exit = false;
	FieldType			*fldLP;
	
	Char				*temp;
	//UInt				i;
	
	FrmPopupForm(frmCustomLongPress);
	
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		
		switch (event.eType)
		{
			case frmLoadEvent:
			{
				form = FrmInitForm(frmCustomLongPress);
				break;
			}
			case frmOpenEvent:
			{
				FrmDrawForm(form);
				FrmSetActiveForm(form);
				
				fldLP = (FieldType *)FrmGetObjectPtr(form, FrmGetObjectIndex(form, fldCustomLP));
				FrmSetControlValue(form, FrmGetObjectIndex(form, cbEnglishPunc), pref->english_punc);
				FrmSetControlValue(form, FrmGetObjectIndex(form, cbFullwidth), pref->fullwidth);
				FrmSetControlValue(form, FrmGetObjectIndex(form, cbOptFullwidth), pref->opt_fullwidth);
				FrmSetControlValue(form, FrmGetObjectIndex(form, cbNumFullwidth), pref->num_fullwidth);
				if(!pref->fullwidth)
				{
					FrmHideObject(form, FrmGetObjectIndex(form, cbOptFullwidth));
					FrmHideObject(form, FrmGetObjectIndex(form, cbNumFullwidth));
				}
				FrmSetTitle(form,(Char*)CtlGetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, btnCustomLPQ))));
				SetFieldTextFromStr (fldLP, pref->CustomLP[16], true);
				temp = pref->CustomLP[16];
				
				break;
			}
			case frmUpdateEvent:
			{
				FrmDrawForm(form);
				break;
			}
			case ctlSelectEvent:
			{
				switch (event.data.ctlSelect.controlID)
				{
					case btnCustomLPPeriod:
					{
						FrmSetTitle(form,(Char*)CtlGetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, btnCustomLPPeriod))));
						SetFieldTextFromStr (fldLP, pref->CustomLPPeriod, true);
						temp = pref->CustomLPPeriod;
						
						break;
					}			
					case btnCustomLPOptBackspace:
					{
						FrmSetTitle(form,(Char*)CtlGetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, btnCustomLPOptBackspace))));
						SetFieldTextFromStr (fldLP, pref->CustomLPOptBackspace, true);
						temp = pref->CustomLPOptBackspace;
						
						break;
					}
					case btnCustomLPShiftBackspace:
					{
						FrmSetTitle(form,(Char*)CtlGetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, btnCustomLPShiftBackspace))));
						SetFieldTextFromStr (fldLP, pref->CustomLPShiftBackspace, true);
						temp = pref->CustomLPShiftBackspace;
						
						break;
					}
					case btnCustomLPShiftPeriod:
					{
						FrmSetTitle(form,(Char*)CtlGetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, btnCustomLPShiftPeriod))));
						SetFieldTextFromStr (fldLP, pref->CustomLPShiftPeriod, true);
						temp = pref->CustomLPShiftPeriod;
						
						break;
					}
					case btnCustomLPSave:
					{
						StrCopy(temp, FldGetTextPtr(fldLP));
						break;
					}
					case cbEnglishPunc: //英文标点
					{
						pref->english_punc = ! pref->english_punc;
						break;
					}
					case cbFullwidth: //全角符号
					{
						pref->fullwidth = ! pref->fullwidth;
						if(pref->fullwidth)
						{
							FrmShowObject(form, FrmGetObjectIndex(form, cbOptFullwidth));
							FrmShowObject(form, FrmGetObjectIndex(form, cbNumFullwidth));
						}
						else
						{
							FrmHideObject(form, FrmGetObjectIndex(form, cbOptFullwidth));
							FrmHideObject(form, FrmGetObjectIndex(form, cbNumFullwidth));
						}
						break;
					}
					case cbOptFullwidth: //Opt是否输出全角符号
					{
						pref->opt_fullwidth = ! pref->opt_fullwidth;
						break;
					}
					case cbNumFullwidth: //全角数字
					{
						pref->num_fullwidth = ! pref->num_fullwidth;
						break;
					}																	
					case btnLPOK:
					{
						exit = true;
						break;
					}
					case btnLPRestoreDefault:
					{
						ResetSign(pref);
						SetFieldTextFromStr (fldLP, temp, true);
						FldDrawField(fldLP);
						break;
					}
					default:
					{
						Int16 idx =  event.data.ctlSelect.controlID - btnCustomLPA;
						if(idx>=0 && idx<=25)
						{
							Char title[2]="A";
							title[0]='A'+(Char)idx;
							FrmSetTitle(form, title);
							SetFieldTextFromStr (fldLP, pref->CustomLP[idx], true);
							temp = pref->CustomLP[idx];
						}						
						break;						
					}
				}
				FrmUpdateForm(frmCustomLongPress, 0);
				break;
			}
			default:
			{
				if (! SysHandleEvent(&event))
				{
					FrmDispatchEvent(&event);
				}
				break;
			}
		}
	}while (event.eType != appStopEvent && (! exit));
	//保存设置
	PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	//返回
	FrmReturnToForm(0);
}
//
static void * GetObjectPtr(FormPtr form, UInt16 objectID)
{
    return FrmGetObjectPtr(form, FrmGetObjectIndex(form, objectID));
}

//--------------------------------------------------------------------------
//自定义界面
static void CustomDisplayEventHandler(stru_Pref *pref)
{
	EventType			event;
	FormType			*form;
	Boolean				exit = false;
	ListType			*lstP;
	ControlType			*triP;
	FieldType			*fldPX;
	FieldType			*fldPY;
	char				strShow[maxStrIToALen];
	
	FrmPopupForm(frmCustomDisplay);
	
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		
		switch (event.eType)
		{
			case frmLoadEvent:
			{
				form = FrmInitForm(frmCustomDisplay);
				//FrmSetActiveForm(form);
				break;
			}
			case frmOpenEvent:
			{
				FrmDrawForm(form);
				FrmSetActiveForm(form);
				PaintCurrentColor(pref);
				
				lstP = GetObjectPtr(form,listJavaStatusStyle);
				triP = GetObjectPtr(form,triggerJavaStatusStyle);
				fldPX = (FieldType *)GetObjectPtr(form, fldStyleX);
				fldPY = (FieldType *)GetObjectPtr(form, fldStyleY);
			
				LstSetSelection(lstP,pref->javaStatusStyle);
				CtlSetLabel(triP,LstGetSelectionText(lstP,pref->javaStatusStyle));
				
				//仅Java模式显示中英提示
				FrmSetControlValue(form, FrmGetObjectIndex(form, cbOnlyJavaModeShow),pref->onlyJavaModeShow);
				
				if(pref->javaStatusStyle == 0)
				{
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
					FrmHideObject(form, FrmGetObjectIndex(form, fldStyleX));
					FrmHideObject(form, FrmGetObjectIndex(form, lblStyleX));
					FrmHideObject(form, FrmGetObjectIndex(form, lblP1));
					FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
					FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
					FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
				}
				else if(pref->javaStatusStyle == 1) 
				{
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
					FrmHideObject(form, FrmGetObjectIndex(form, fldStyleX));
					FrmHideObject(form, FrmGetObjectIndex(form, lblStyleX));
					FrmHideObject(form, FrmGetObjectIndex(form, lblP1));
					FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
					FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
					FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
				}
				else if(pref->javaStatusStyle == 2) 
				{
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
					FrmShowObject(form, FrmGetObjectIndex(form, fldStyleX));
					FrmShowObject(form, FrmGetObjectIndex(form, lblStyleX));
					FrmShowObject(form, FrmGetObjectIndex(form, lblP1));
					FrmShowObject(form, FrmGetObjectIndex(form, lblStyleY));
					FrmShowObject(form, FrmGetObjectIndex(form, fldStyleY));
					FrmShowObject(form, FrmGetObjectIndex(form, lblP2));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
					FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
				}
				else if(pref->javaStatusStyle == 3)
				{
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
					FrmShowObject(form, FrmGetObjectIndex(form, fldStyleX));
					FrmShowObject(form, FrmGetObjectIndex(form, lblStyleX));
					FrmShowObject(form, FrmGetObjectIndex(form, lblP1));
					FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
					FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
					FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
					FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
				}
				FldInsert(fldPX, StrIToA(strShow,pref->javaStatusStyleX), StrLen(StrIToA(strShow,pref->javaStatusStyleX)));
				FldInsert(fldPY, StrIToA(strShow,pref->javaStatusStyleY), StrLen(StrIToA(strShow,pref->javaStatusStyleY)));
				
				break;
			}
			case frmUpdateEvent:
			{
				FrmDrawForm(form);
				PaintCurrentColor(pref);
				break;
			}
			case ctlSelectEvent:
			{
				switch (event.data.ctlSelect.controlID)
				{
					case triggerJavaStatusStyle:
					{
						LstPopupList(lstP);
						CtlSetLabel(triP,LstGetSelectionText(lstP,LstGetSelection(lstP)));
						pref->javaStatusStyle = LstGetSelection (lstP);
						if(pref->javaStatusStyle == 0)
						{
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
							FrmHideObject(form, FrmGetObjectIndex(form, fldStyleX));
							FrmHideObject(form, FrmGetObjectIndex(form, lblStyleX));
							FrmHideObject(form, FrmGetObjectIndex(form, lblP1));
							FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
							FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
							FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
						}
						else if(pref->javaStatusStyle == 1) 
						{
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
							FrmHideObject(form, FrmGetObjectIndex(form, fldStyleX));
							FrmHideObject(form, FrmGetObjectIndex(form, lblStyleX));
							FrmHideObject(form, FrmGetObjectIndex(form, lblP1));
							FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
							FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
							FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
						}
						else if(pref->javaStatusStyle == 2) 
						{
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
							FrmShowObject(form, FrmGetObjectIndex(form, fldStyleX));
							FrmShowObject(form, FrmGetObjectIndex(form, lblStyleX));
							FrmShowObject(form, FrmGetObjectIndex(form, lblP1));
							FrmShowObject(form, FrmGetObjectIndex(form, lblStyleY));
							FrmShowObject(form, FrmGetObjectIndex(form, fldStyleY));
							FrmShowObject(form, FrmGetObjectIndex(form, lblP2));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
							FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
						}
						else if(pref->javaStatusStyle == 3)
						{
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
							FrmShowObject(form, FrmGetObjectIndex(form, fldStyleX));
							FrmShowObject(form, FrmGetObjectIndex(form, lblStyleX));
							FrmShowObject(form, FrmGetObjectIndex(form, lblP1));
							FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
							FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
							FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
							FrmShowObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));
						}
						break;
					}
					case btnCustomCaret:
					{
						UIPickColor(NULL, &pref->caretColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomFrame:
					{
						UIPickColor(NULL, &pref->frameColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomCodeFore:
					{
						UIPickColor(NULL, &pref->codeForeColor, UIPickColorStartRGB,CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomCodeBG:
					{
						UIPickColor(NULL, &pref->codeBackColor, UIPickColorStartRGB,CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomResultFore:
					{
						UIPickColor(NULL, &pref->resultForeColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomResultBackBG:
					{
						UIPickColor(NULL, &pref->resultBackColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomResultHFore:
					{
						UIPickColor(NULL, &pref->resultHighlightForeColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomResultHBackBG:
					{
						UIPickColor(NULL, &pref->resultHighlightBackColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomChineseStatus:
					{
						UIPickColor(NULL, &pref->chineseStatusColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomEnglishStatus:
					{
						UIPickColor(NULL, &pref->englishStatusColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomChineseEdge:
					{
						UIPickColor(NULL, &pref->chineseEdgeColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case btnCustomEnglishEdge:
					{
						UIPickColor(NULL, &pref->englishEdgeColor, UIPickColorStartRGB, CtlGetLabel(event.data.ctlSelect.pControl), NULL);
						break;
					}
					case cbOnlyJavaModeShow:
					{
						pref->onlyJavaModeShow = (! pref->onlyJavaModeShow);
						break;
					}
					case btnCustomOK:
					{
						pref->javaStatusStyleX = (UInt8)StrAToI(FldGetTextPtr(fldPX));
						pref->javaStatusStyleY = (UInt8)StrAToI(FldGetTextPtr(fldPY));
						exit = true;
						break;
					}
					case btnDefaultDisplay:
					{
						pref->onlyJavaModeShow = true;//仅Java模式显示中英文提示
						FrmSetControlValue(form, FrmGetObjectIndex(form, cbOnlyJavaModeShow),pref->onlyJavaModeShow);//仅Java模式显示中英文提示
						pref->javaStatusStyle = Style1;	//Java、DTG状态的指示样式
						lstP = FrmGetObjectPtr(form,FrmGetObjectIndex(form,listJavaStatusStyle));//列表恢复默认
						LstSetSelection(lstP,pref->javaStatusStyle);//列表恢复默认
						CtlSetLabel(triP,LstGetSelectionText(lstP,pref->javaStatusStyle));//列表恢复默认
						pref->javaStatusStyleX = 20;//Java、DTG状态的指示样式之矩阵的默认宽度
						pref->javaStatusStyleY = 20;//Java、DTG状态的指示样式之矩阵的默认高度
						
						//界面恢复默认
						FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishStatus));
						FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseStatus));
						FrmHideObject(form, FrmGetObjectIndex(form, fldStyleX));
						FrmHideObject(form, FrmGetObjectIndex(form, lblStyleX));
						FrmHideObject(form, FrmGetObjectIndex(form, lblP1));
						FrmHideObject(form, FrmGetObjectIndex(form, lblStyleY));
						FrmHideObject(form, FrmGetObjectIndex(form, fldStyleY));
						FrmHideObject(form, FrmGetObjectIndex(form, lblP2));
						FrmHideObject(form, FrmGetObjectIndex(form, btnCustomChineseEdge));
						FrmHideObject(form, FrmGetObjectIndex(form, btnCustomEnglishEdge));					
						
						//色彩恢复默认
						pref->chineseStatusColor.r = 255;
						pref->chineseStatusColor.g = 0;
						pref->chineseStatusColor.b = 0;
						pref->englishStatusColor.r = 0;
						pref->englishStatusColor.g = 0;
						pref->englishStatusColor.b = 255;
						pref->chineseEdgeColor.r = 128;
						pref->chineseEdgeColor.g = 0;
						pref->chineseEdgeColor.b = 0;
						pref->englishEdgeColor.r = 0;
						pref->englishEdgeColor.g = 0;
						pref->englishEdgeColor.b = 128;
						UIColorGetTableEntryRGB(UIFormFrame, &pref->caretColor);								//光标
						UIColorGetTableEntryRGB(UIDialogFrame, &pref->frameColor);								//边框
						UIColorGetTableEntryRGB(UIObjectForeground, &pref->codeForeColor);						//关键字颜色
						UIColorGetTableEntryRGB(UIDialogFill, &pref->codeBackColor);							//关键字背景
						UIColorGetTableEntryRGB(UIObjectForeground, &pref->resultForeColor);					//待选字颜色
						UIColorGetTableEntryRGB(UIObjectFill, &pref->resultBackColor);							//待选字背景
						UIColorGetTableEntryRGB(UIObjectSelectedForeground, &pref->resultHighlightForeColor);	//待选字高亮颜色
						UIColorGetTableEntryRGB(UIObjectSelectedFill, &pref->resultHighlightBackColor);			//待选字高亮背景
						if (pref->resultBackColor.r > 245 && pref->resultBackColor.g > 245 && pref->resultBackColor.b > 245)
						{
							pref->resultBackColor = pref->resultHighlightBackColor;
							if (pref->resultBackColor.r > 85)
							{
								pref->resultBackColor.r = 255;
							}
							else
							{
								pref->resultBackColor.r += 170;
							}
							if (pref->resultBackColor.g > 85)
							{
								pref->resultBackColor.g = 255;
							}
							else
							{
								pref->resultBackColor.g += 170;
							}
							if (pref->resultBackColor.b > 85)
							{
								pref->resultBackColor.b = 255;
							}
							else
							{
								pref->resultBackColor.b += 170;
							}
						}
						break;
					}
				}
				FrmUpdateForm(frmCustomDisplay, 0);
				break;
			}
			default:
			{
				if (! SysHandleEvent(&event))
				{
					FrmDispatchEvent(&event);
				}
				break;
			}
		}
	}while (event.eType != appStopEvent && (! exit));
	//保存设置
	PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	//返回
	FrmReturnToForm(0);
}
//
//初始化默认设置
static void DefaultPref(stru_Pref	*pref)
{
	UInt16			periodP;
	UInt16			doubleTapDelayP;
	UInt32			company_id;
	UInt32			device_id;
	Boolean			queueAheadP;
		
	pref->Enabled = false;
	pref->Actived = false;
	pref->NotifyPriority = 0;
	//机器型号
	FtrGet(sysFtrCreator, sysFtrNumOEMCompanyID, &company_id);
	FtrGet(sysFtrCreator, sysFtrNumOEMDeviceID, &device_id);
	
	if ((company_id == 'Palm' || company_id == 'hspr') && 
		(device_id == 'H102' || device_id == 'H202' || device_id == 'D053'))//Treo650,680
	{
		pref->isTreo = isTreo650;	
	}
	else if ((company_id == 'Palm' || company_id == 'hspr') && 
		(device_id == 'D061' || device_id == 'D052'))//Centro,Treo700P
	{
		pref->isTreo = isTreo650;
	}
	else if (company_id == 'hspr' && (device_id == 'H101' || device_id == 'H201')) //Treo600
	{
		pref->isTreo = isTreo600;
	}
	else pref->isTreo = 0;
	
	//默认字体
	pref->displayFont = boldFont;
	//默认按键延迟
	KeyRates(false, &pref->defaultKeyRate, &periodP, &doubleTapDelayP, &queueAheadP);
	//按键范围
	pref->keyRange[0] = keyA;
	pref->keyRange[1] = keyZ;
	//默认按键
	pref->Selector[0] = 0x0020; pref->Selector[1] = keyZero; pref->Selector[2] = hsKeySymbol;
	pref->Selector[3] = keyLeftShift; pref->Selector[4] = keyRightShift;
	//默认按键2
	pref->Selector2[0] = 0; pref->Selector2[1] = 0; pref->Selector2[2] = 0;
	pref->Selector2[3] = 0; pref->Selector2[4] = 0;
	//按键模式
	pref->KBMode = KBModeTreo;
	//特殊按键
	pref->IMESwitchKey = keyHard1;
	pref->JavaActiveKey = keySpace;
	pref->KBMBSwitchKey = 0;
	pref->MBSwitchKey = 0;
	pref->TempMBSwitchKey = 0;	
	pref->PuncKey = 0;
	pref->PuncType = 0;
	pref->ListKey = hsKeySymbol;
	pref->SyncopateKey = keyPeriod;
	//设置输入法状态
	pref->shouldShowfloatBar = true;
	pref->DTGSupport = false;
	pref->AutoMBSwich = false;
	pref->LongPressMBSwich = true;
	//启动方式
	pref->init_mode = initDefaultChinese;
	//最后一次输入法状态
	pref->last_mode = imeModeChinese;
	pref->init_mode_record = 0;
	pref->current_field = NULL;
	pref->field_in_table = false;
	//初始按键检测参数
	pref->hasShiftMask = false;
	pref->hasOptionMask = false;
	pref->isLongPress = false;
	pref->longPressHandled = false;
	//默认光标颜色
	UIColorGetTableEntryRGB(UIFieldCaret, &pref->defaultCaretColor);
	//当前窗口
	pref->curWin = NULL;
	//默认界面
	pref->onlyJavaModeShow = true;
	UIColorGetTableEntryRGB(UIFormFrame, &pref->caretColor);								//光标
	UIColorGetTableEntryRGB(UIDialogFrame, &pref->frameColor);								//边框
	UIColorGetTableEntryRGB(UIObjectForeground, &pref->codeForeColor);						//关键字颜色
	UIColorGetTableEntryRGB(UIDialogFill, &pref->codeBackColor);							//关键字背景
	UIColorGetTableEntryRGB(UIObjectForeground, &pref->resultForeColor);					//待选字颜色
	UIColorGetTableEntryRGB(UIObjectFill, &pref->resultBackColor);							//待选字背景
	UIColorGetTableEntryRGB(UIObjectSelectedForeground, &pref->resultHighlightForeColor);	//待选字高亮颜色
	UIColorGetTableEntryRGB(UIObjectSelectedFill, &pref->resultHighlightBackColor);			//待选字高亮背景
	
	pref->chineseStatusColor.r = 255;
	pref->chineseStatusColor.g = 0;
	pref->chineseStatusColor.b = 0;
	pref->englishStatusColor.r = 0;
	pref->englishStatusColor.g = 0;
	pref->englishStatusColor.b = 255;
	pref->chineseEdgeColor.r = 128;
	pref->chineseEdgeColor.g = 128;
	pref->chineseEdgeColor.b = 128;
	pref->englishEdgeColor.r = 128;
	pref->englishEdgeColor.g = 128;
	pref->englishEdgeColor.b = 128;
	pref->javaStatusStyle = Style1;	//Java、DTG状态的指示样式
	pref->javaStatusStyleX = 20;//Java、DTG状态的指示样式之矩阵的默认宽度
	pref->javaStatusStyleY = 20;//Java、DTG状态的指示样式之矩阵的默认高度
	if (pref->resultBackColor.r > 245 && pref->resultBackColor.g > 245 && pref->resultBackColor.b > 245)
	{
		pref->resultBackColor = pref->resultHighlightBackColor;
		if (pref->resultBackColor.r > 85)
		{
			pref->resultBackColor.r = 255;
		}
		else
		{
			pref->resultBackColor.r += 170;
		}
		if (pref->resultBackColor.g > 85)
		{
			pref->resultBackColor.g = 255;
		}
		else
		{
			pref->resultBackColor.g += 170;
		}
		if (pref->resultBackColor.b > 85)
		{
			pref->resultBackColor.b = 255;
		}
		else
		{
			pref->resultBackColor.b += 170;
		}
	}
	//动态加载
	pref->dync_load = false;
	pref->autoSend = true;
	pref->filterGB = false;
	pref->filterChar = false;
	pref->suggestChar = false;
	pref->altChar = false;
	pref->extractChar = false;
	pref->english_punc = false;
	pref->fullwidth = false;
	pref->opt_fullwidth = false;
	pref->num_fullwidth = false;
	pref->choice_button = false;
	pref->menu_button = false;
	pref->showGsi = true;		
	ResetSign(pref);//自定义符号
}
//
//检测汉字信息是否存在
static Boolean IsDictExist(void)
{
	DmOpenRef db_ref = NULL;	
	FileRef db_file_ref = NULL;
	Boolean exist=false;
	//打开汉字信息数据库
	db_ref = DmOpenDatabaseByTypeCreator('dict', 'pIME', dmModeReadOnly);
	if (db_ref)
	{
		DmCloseDatabase(db_ref);
		exist = true;
	}
	else
	{
		UInt16 vol_ref;
		UInt32 vol_iterator = vfsIteratorStart;	
		while (vol_iterator != vfsIteratorStop)//获取储存卡引用,取卡指针
		{
			VFSVolumeEnumerate(&vol_ref, &vol_iterator);
		}	
		if(vol_ref > 0)//在内存上没找到数据库，尝试在卡上找
		{
			 if(VFSFileOpen(vol_ref, PIME_CARD_PATH_DICT, vfsModeRead, &db_file_ref) == errNone)
			 {
			 	VFSFileClose(db_file_ref);
				exist = true;	
			 }
		}
	}
	
	return exist;
}
//--------------------------------------------------------------------------
//设置界面
static void MainFormEventHandler(Boolean IsDA)
{
	UInt16			error;
	UInt16			cardNo;
	UInt16			i;
	UInt16			mb_num;
	UInt16			pref_size;
	UInt32			pref_address;
	UInt32			db_type;
	UInt32			db_type_appl = sysFileTApplication;
	UInt32			db_type_panl = sysFileTPanel;
	Boolean			LaunchFromPref;
	Boolean			pref_exist;
	Char			**mb_list;
	stru_Pref		*pref = NULL;
	stru_MBInfo		mb_info;
	EventType		event;
	EventType		ep;
	FormType		*frmP;
	ListType		*lstP;
	ListType		*lstPopP;
	LocalID			dbID;
	

	pref_size = sizeof(stru_Pref); //pref尺寸
	//是否有已存在的pref指针
	if (FtrGet(appFileCreator, ftrPrefNum, &pref_address) == ftrErrNoSuchFeature)
	{
		pref = (stru_Pref *)MemPtrNew(pref_size);
		MemPtrSetOwner(pref, 0); //设置为系统所有
		MemSet(pref, pref_size, 0x00); //清零
		pref_address = (UInt32)pref;
		FtrSet(appFileCreator, ftrPrefNum, pref_address);
		pref_exist = false;
	}
	else
	{
		pref = (stru_Pref *)pref_address;
		pref_exist = true;
	}
	MemSet(&mb_info, sizeof(stru_MBInfo), 0x00);
	//检测从哪里启动的
	SysCurAppDatabase(&cardNo, &dbID);
	DmDatabaseInfo(cardNo, dbID, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &db_type, NULL);
	LaunchFromPref = (db_type == sysFileTPanel);
	do 
	{
		EvtGetEvent(&event, evtWaitForever);

		if (! SysHandleEvent(&event))
		{
			if (! MenuHandleEvent(0, &event, &error))
			{
				switch (event.eType) 
				{
					case menuEvent:
					{
						switch (event.data.menu.itemID)
						{
							case OptionShowInLauncher:
							{
								SysCurAppDatabase(&cardNo, &dbID);
								DmSetDatabaseInfo(cardNo, dbID, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &db_type_appl, NULL);
								LaunchFromPref = false;
								FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnBackToPref));
								break;
							}
							case OptionShowInPref:
							{
								SysCurAppDatabase(&cardNo, &dbID);
								DmSetDatabaseInfo(cardNo, dbID, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &db_type_panl, NULL);
								LaunchFromPref = true;
								FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnBackToPref));
								break;
							}
							case OptionsAboutPocketIME:
							{

								MenuEraseStatus(0);				

								frmP = FrmInitForm(AboutForm);
								FrmDoDialog (frmP);                    
								FrmDeleteForm (frmP);
								frmP = FrmGetActiveForm();
								break;
							}
							case OptionCustomDisplay:
							{
								CustomDisplayEventHandler(pref);
								break;
							}
							case OptionCustomFont:
							{
								pref->displayFont = FontSelect(pref->displayFont);
								break;
							}							
							case OptionHelp:
							{
								FrmHelp(1000);
								break;
							}
							case OptionAdvSetting:
							{
								AdvanceSettingEventHandler(pref);
								break;
							}
							case OptionCustomLongPress:
							{
								CustomLongPressEventHandler(pref);
								break;
							}
						}
						break;
					}
					case frmLoadEvent:
					{
						frmP = FrmInitForm(MainForm);
						FrmSetActiveForm(frmP);
						break;
					}
					case frmOpenEvent:
					{						
						frmP = FrmGetActiveForm();									
						lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstMBList)); //码表列表指针
						lstPopP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstInitMode)); //启动方式列表指针
						//保存装载到内存中的储存卡码表
						UnloadMB(unloadAll, true);
						//更新码表信息数据库，获取码表数及码表列表
						mb_num = UpdateMBListDB(&mb_list);							

						//装载pref
						if (! pref_exist)
						{
							if (PrefGetAppPreferences(appFileCreator, appPrefID, pref, &pref_size, true) == noPreferenceFound)
							{
								DefaultPref(pref);
							}
							//初始化码表信息
							MemSet(&pref->curMBInfo, sizeof(stru_MBInfo), 0x00);
							//其他
							pref->keyDownDetected = false;
							//PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
						}
						pref->activeStatus &= (~optActiveJavaMask);
						PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
						
						if (pref->Enabled)
						{
							MemPtrSetOwner(pref, 0);
						}
						if(IsDA)//DA运行后切换启动 然后退出
						{					
							pref->Enabled = !pref->Enabled;
							ShowStatus(pref->Enabled ?frmEnable:frmDisable, NULL, 500);							
							FrmReturnToForm(0);							
							MemSet(&ep, sizeof(EventType), 0x00);
							ep.eType = appStopEvent;
							EvtAddEventToQueue(&ep);	
							break;
						}
						if (mb_num > 0)
						{
							LstSetListChoices(lstP, mb_list, mb_num);
							//LstDrawList(lstP);
							LstSetSelection(lstP, noListSelection);
						}else
							FrmAlert(alertNoMabiao);												
						SetInitModeTrigger((Int16)pref->init_mode, pref);
						FrmSetFocus(frmP, FrmGetObjectIndex(frmP, pbtnEnable));
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnEnable), pref->Enabled);						
						
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbDyncLoad), pref->dync_load);
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbFilterGB), pref->filterGB);
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbFilterChar), pref->filterChar);
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbExtractChar), pref->extractChar);
						FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbAutoSend), pref->autoSend);
						if(IsDictExist())//检测汉字信息是否存在，存在则设置值，否则隐藏
						{
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbAltChar), pref->altChar);
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbSuggestChar), pref->suggestChar);
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, cbAltChar));
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, cbSuggestChar));
						}
						else
						{
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, cbAltChar));
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, cbSuggestChar));
							pref->ListKey = 0;
						}
						if (LaunchFromPref)
						{
							FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnBackToPref));
						}
						else
						{
							FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnBackToPref));
						}
						if (mb_num > 0)
						{
							LstSetSelection(lstP, 0);
							MemSet(&ep, sizeof(EventType), 0x00);
							ep.eType = lstSelectEvent;
							ep.data.lstSelect.listID = lstMBList;
							ep.data.lstSelect.pList = lstP;
							ep.data.lstSelect.selection = 0;
							EvtAddEventToQueue(&ep);
						}
						FrmDrawForm(frmP);																	
						break;
       	 			}
					case ctlSelectEvent:
					{
						switch (event.data.ctlSelect.controlID)
						{
							case btnMBDelete: //删除码表
							{
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, btnMBDelete), 0);
								if (mb_num > 0) //有码表可以删除
								{
									//保存装载到内存中的储存卡码表
									UnloadMB(unloadAll, true);
									//删除码表
									DeleteMB(LstGetSelection(lstP));
									for (i = 0; i < mb_num; i ++)
									{
										MemPtrFree(mb_list[i]);
									}
									if (mb_list != NULL)
									{
										MemPtrFree(mb_list);
									}
									//更新码表信息数据库，获取码表数及码表列表
									mb_num = UpdateMBListDB(&mb_list);
									if (mb_num > 0)
									{
										LstSetListChoices(lstP, mb_list, mb_num);
										LstDrawList(lstP);
										LstSetSelection(lstP, 0);
										MemSet(&ep, sizeof(EventType), 0x00);
										ep.eType = lstSelectEvent;
										ep.data.lstSelect.listID = lstMBList;
										ep.data.lstSelect.pList = lstP;
										ep.data.lstSelect.selection = 0;
										EvtAddEventToQueue(&ep);
									}
									else
									{
										LstSetListChoices(lstP, NULL, 0);
										LstDrawList(lstP);
										LstSetSelection(lstP, noListSelection);
									}
								}
								break;
							}
							case ptriInitMode: //启动方式设置
							{
								SetInitModeTrigger(LstPopupList(lstPopP), pref);
								break;
							}
							case btnSetBlur: //设置模糊音
							{
								if (LstGetSelection(lstP) != noListSelection)
								{
									SetBlurEventHandler(pref, (UInt16)LstGetSelection(lstP));
								}
								break;
							}
							case cbDyncLoad: //动态加载码表
							{
								pref->dync_load = ! pref->dync_load;
								break;
							}
							case cbExtractChar: //是否允许以词定字
							{
								pref->extractChar = ! pref->extractChar;
								break;
							}	
							case cbAutoSend: //是否允许自动上屏
							{
								pref->autoSend = ! pref->autoSend;
								break;
							}														
							case cbAltChar: //是否允许字符转换
							{
								pref->altChar = ! pref->altChar;
								break;
							}
							case cbSuggestChar: //是否允许词语联想
							{
								pref->suggestChar = ! pref->suggestChar;
								break;
							}		
							case cbFilterGB: //字符集，是否仅显示GB2312字符
							{
								pref->filterGB = ! pref->filterGB;
								break;
							}
							case cbFilterChar: //词组输入
							{
								pref->filterChar = ! pref->filterChar;
								break;
							}							
							case btnBackToPref: //退出
							{
								MemSet(&event, sizeof(EventType), 0x00);
								event.eType = appStopEvent;
								break;
							}
							case cbEnabledTS: //是否启用词频调整
							{
								if (LstGetSelection(lstP) != noListSelection && (mb_num > 0))
								{
									mb_info.frequency_adjust = (FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cbEnabledTS))) !=0; //启用词频调整
									SetMBInfoByNameType(mb_info.file_name, mb_info.db_type, mb_info.inRAM, &mb_info);
								}
								break;
							}
							case cbDynTips: //渐进查找
							{
								if (LstGetSelection(lstP) != noListSelection && (mb_num > 0))
								{
									mb_info.gradually_search = (FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cbDynTips)))!=0;
									SetMBInfoByNameType(mb_info.file_name, mb_info.db_type, mb_info.inRAM, &mb_info);
								}
								break;
							}
							case cbMBEnable: //启用、停用码表
							{
								if (LstGetSelection(lstP) != noListSelection)
								{
									mb_info.enabled = (Boolean)FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cbMBEnable));
									SetMBInfoFormMBList(&mb_info, (UInt16)LstGetSelection(lstP));
								}
								break;
							}
							case btnMBUp: //上移
							case btnMBDown: //下移
							{
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, event.data.ctlSelect.controlID), 0);
								if ((LstGetSelection(lstP) > 0 && event.data.ctlSelect.controlID==btnMBUp)\
									|| (LstGetSelection(lstP) < LstGetNumberOfItems(lstP) - 1 && event.data.ctlSelect.controlID==btnMBDown))
								{
									MoveMBRecordInMBListDB((UInt16)LstGetSelection(lstP), lstP, &mb_list, event.data.ctlSelect.controlID - btnMBUp);
								}
								break;
							}
							case pbtnEnable: //启停输入法
							{														
								if (pref->Enabled == false) //启用
								{
									//获取码表信息库的一个启用的码表
									for (i = 0; i < mb_num; i ++)
									{
										if (MBEnabled(i))
										{
											pref->Enabled = true;
											break;
										}
									}
								}
								else //停用
								{
									pref->Enabled = false;
								}
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnEnable), pref->Enabled);			
								break;
							}
						}
						break;
					}
					case lstSelectEvent: //显示码表细节
					{
						if (event.data.lstSelect.selection >= 0)
						{
							//获取信息
							GetMBInfoFormMBList(&mb_info, event.data.lstSelect.selection, true, true);
							//是否启用
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbMBEnable), (Int16)mb_info.enabled);
							//是否包含模糊音
							if (mb_info.smart_offset != 0)
							{
								FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetBlur));
							}
							else
							{
								FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetBlur));
							}
							//渐进查找
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbDynTips), mb_info.gradually_search);
							//词频调整
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbEnabledTS), mb_info.frequency_adjust);
						}
						break;
					}
					default :
					{
						FrmDispatchEvent(&event);
						break;
					}
				}
			}
		}
	} while (event.eType != appStopEvent);
	
	//从内存卸载所有被加载的码表
	UnloadMB(unloadAll, true);
	//重构pref指针信息，先释放原pref指针中的自动切音和键值转换内容
	if (pref->curMBInfo.syncopate_offset > 0 && pref->curMBInfo.key_syncopate != NULL)
	{
		MemPtrFree(pref->curMBInfo.key_syncopate);
	}
	if (pref->curMBInfo.translate_offset > 0 && pref->curMBInfo.key_translate != NULL)
	{
		MemPtrFree(pref->curMBInfo.key_translate);
	}
	//清空pref中码表信息的内容
	MemSet(&pref->curMBInfo, sizeof(stru_MBInfo), 0x00);
	SysCurAppDatabase(&cardNo, &dbID);
	if (! pref->Enabled) //输入法未启动，释放全部pref内容，取消所有消息的注册
	{
		FtrUnregister(appFileCreator, ftrPrefNum);
		SetKeyRates(true, pref);
		SetCaretColor(true, pref);
		SysNotifyUnregister(cardNo, dbID, sysNotifyInsPtEnableEvent, sysNotifyNormalPriority);
		SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
		//保存设置
		PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
		MemPtrFree(pref);
	}
	else //输入法被启动，构造pref的码表信息内容
	{
		//获取码表信息库的一个启用的码表
		for (i = 0; i < mb_num; i ++)
		{
			if (MBEnabled(i))
			{
				GetMBInfoFormMBList(&pref->curMBInfo, i, true, pref->dync_load);
				GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load);
				pref->last_mode = imeModeChinese;
				//保存设置
				PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
				
				MemPtrSetOwner(pref, 0);
				SysNotifyRegister(cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				SysNotifyRegister(cardNo, dbID, sysNotifyInsPtEnableEvent, NULL, sysNotifyNormalPriority, pref);
				break;
			}
		}
	}
	
	//释放内存
	for (i = 0; i < mb_num; i ++)
	{
		MemPtrFree(mb_list[i]);
	}
	if (mb_list != NULL)
	{
		MemPtrFree(mb_list);
	}
	if (mb_info.key_syncopate != NULL)
	{
		MemPtrFree(mb_info.key_syncopate);
	}
	if (mb_info.key_translate != NULL)
	{
		MemPtrFree(mb_info.key_translate);
	}
	//返回Launcher还是控制台
	if (IsDA)
	{
		//FrmEraseForm( frmP );
        FrmDeleteForm( frmP );
	}
	else if(LaunchFromPref)
	{
		LaunchWithCommand(sysFileTApplication, sysFileTPreferences, sysAppLaunchCmdNormalLaunch, NULL);
	}
}
//--------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////////////////