#include <PalmOS.h>
#include <68K\Hs.h>
#include <common\system\palmOneNavigator.h>
#include <common\system\HsKeyCodes.h>
#include <HsKeyTypes.h>

#include "PocketIME.h"
#include "PocketIME_Rsc.h"

//---------------------��������----------------------------------------------------------------
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
        DmOpenRef dbP,        // (in)��Դ�ļ����ݿ�ָ��
        UInt16 uwBitmapIndex, // (in)λͼ��Դ��Index����ID
        Coord x,              // (in)λͼ���Ͻǵ�x����
        Coord y,              // (in)λͼ���Ͻǵ�y����
        Boolean bByIndex      // (in)true��������Դ��������ȡBitmap
                              //     false��������ԴID����ȡBitmap
                              // ���Ϊtrue��������dbP����
);

static FieldType *GetActiveField(stru_Pref *pref);
#pragma mark -
//////////////////////////////////////////////////////////////////////////////////////////////

//---------------------�ں�ģ��----------------------------------------------------------------
//��ʼ�����˫������
static void InitResult(stru_Globe *globe)
{
	stru_Result		*result_ahead;
	
	if (globe->result_head.next != (void *)&globe->result_tail) //�нڵ㣬ɾ��
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
	//������ǰ�ڵ�
	globe->result = NULL;
}
//--------------------------------------------------------------------------
//�½��������ڵ�
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
//��ʼ����������ļ�¼��ѭ������
static void InitMBRecord(stru_Globe *globe)
{
	stru_MBRecord		*mb_record_to_delete;
	stru_ContentOffset	*content;
	stru_ContentOffset	*content_to_delete;
	
	if (globe->mb_record_head != NULL)
	{
		//�ж�ѭ������
		((stru_MBRecord *)globe->mb_record_head->prev)->next = NULL;
		//�ӱ�ͷ��ʼɾ�����нڵ�
		globe->mb_record = (stru_MBRecord *)globe->mb_record_head;
		do
		{
			mb_record_to_delete = globe->mb_record;
			globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
			//ɾ��ƫ��������
			content = (stru_ContentOffset *)mb_record_to_delete->offset_head.next;
			while (content != &mb_record_to_delete->offset_tail)
			{
				content_to_delete = content;
				content = (stru_ContentOffset *)content->next;
				MemPtrFree(content_to_delete);
			}
			//ɾ���ڵ�
			MemPtrFree(mb_record_to_delete);
		}while(globe->mb_record != NULL);
		//ˢ�½ڵ�
		globe->mb_record_head = NULL;
		globe->mb_record = NULL;
	}
}
//--------------------------------------------------------------------------
//�½���������ļ�¼��ѭ������Ľڵ�
static void NewMBRecord(stru_Globe *globe)
{
	stru_MBRecord	*new_mb_record;
	
	if (globe->mb_record_head != NULL) //����Ҫ��ʼ��
	{
		//�����½ڵ���ڴ�
		new_mb_record = (stru_MBRecord *)MemPtrNew(stru_MBRecord_length);
		MemSet(new_mb_record, stru_MBRecord_length, 0x00);
		new_mb_record->offset_head.next = (void *)&new_mb_record->offset_tail;
		//����ָ��
		new_mb_record->next = globe->mb_record->next;
		((stru_MBRecord *)globe->mb_record->next)->prev = (void *)new_mb_record;
		globe->mb_record->next = (void *)new_mb_record;
		new_mb_record->prev = (void *)globe->mb_record;
		//ˢ�µ�ǰ�ڵ�
		globe->mb_record = new_mb_record;
	}
	else //��ʼ��
	{
		//�����ڴ�
		globe->mb_record_head = (stru_MBRecord *)MemPtrNew(stru_MBRecord_length);
		MemSet(globe->mb_record_head, stru_MBRecord_length, 0x00);
		globe->mb_record_head->offset_head.next = (void *)&globe->mb_record_head->offset_tail;
		//����ָ��
		globe->mb_record_head->next = (void *)globe->mb_record_head;
		globe->mb_record_head->prev = (void *)globe->mb_record_head;
		//ˢ�µ�ǰ�ڵ�
		globe->mb_record = globe->mb_record_head;
	}
}
//--------------------------------------------------------------------------
//ɾ����������ļ�¼��ѭ������Ľڵ�
static void DeleteMBRecord(stru_MBRecord *mb_record_to_delete, stru_Globe *globe)
{
	stru_ContentOffset	*content;
	stru_ContentOffset	*content_to_delete;
	
	if (mb_record_to_delete != globe->mb_record_head)
	{
		((stru_MBRecord *)(mb_record_to_delete->prev))->next = mb_record_to_delete->next;
		((stru_MBRecord *)(mb_record_to_delete->next))->prev = mb_record_to_delete->prev;
		//ɾ��ƫ��������
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
//�����������ȡ��¼
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
//����������ͷż�¼
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
//����������ر����ݿ�
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
//�ж��Ƿ�ΪGBK����
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
//��ȡ����м�ֵ�����ݵĳ���
static UInt16 GetLengthOfResultKey(Boolean filterGB, Boolean filterChar, Char *result, UInt16 *key_length, UInt16 *content_length)
{
	UInt16		key_count = 1;
	Char *content;
	
	(*key_length) = 0;
	(*content_length) = 0;
	//��ֵ����
	while ((UInt8)(*result) <= 0x7F && (UInt8)(*result) >0x20)
	{
		if (*result == '\'')
		{
			key_count ++;
		}
		result ++;
		(*key_length) ++;
	}
	//���ݳ���
	content = result;
	while ((UInt8)(*result) > 0x02) //0x00��ȫ���ݽ�����0x01�������ݶν�����0x02�������ݶν������̶����ݣ�
	{
		result ++;
		(*content_length) ++;
	}
	if(filterChar && ((*content_length)>=4))	//�Ƿ�Ϊ����
		return 0;
	if(filterGB && IsGBK( content, (*content_length)))//�Ƿ����ʾGB2312�ַ�
		return 0;
	return key_count;
}
//-----------------------------------
//�����ӳ���
/*static void SubLaunch(const Char *nameP)
{
	  LocalID  dbID = DmFindDatabase(0, nameP);
	  if (dbID)
	    SysAppLaunch(0, dbID, 0, 60000, NULL, NULL);//60000,50011,50012,50013
}*/

//--------------------------------------------------------------------------
//����Ƿ���ת��Ϊȫ��
static void TreoKBFullwidth(Char *str)
{
	if(StrLen(str)==1 && (UInt8)str[0]<0x7F)
	{
		MemHandle  rscHandle;
		Char       *rsc;	
		rscHandle = DmGetResource(strRsc, StrFullwidth) ;
		rsc = MemHandleLock(rscHandle) ;		
		StrNCopy(str, rsc+2*((UInt8)str[0] - ' '), 2); //���ַ����ж�ȡȫ���ַ�
		MemHandleUnlock(rscHandle);
		DmReleaseResource(rscHandle);
	}
}
//
//��̬���� Ŀǰ��֧�������ǰ����ʱ��
static void TreoKBDynamicPunc(Char *str)
{
	switch (str[0])
	{
		case '?': //�����̬����
		{
			switch (str[1])
			{				
				case 'd'://��ǰ����
				case 't'://��ǰʱ��
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
		/*case '!'://�����ӳ���
		{
			SubLaunch(&str[1]);
			MemSet(str, 15, 0x00);
			break;
		}*/
	}
}
//--------------------------------------------------------------------------
//�������86/98���Զ�����
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
		if((type & 0xFFFF) == '86') //���86��
		{
			((UInt16 *)code)[8]='gi';//��
			((UInt16 *)code)[14]='yl';//Ϊ		
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
//���ݼ�ֵת���ַ�����ȡ��ָ�����ݵ��ַ�
static Char *KeyTranslate(Char key, Char *sample, UInt8 mode)
{
	if(mode == GetTranslatedKey || mode ==  GetKeyToShow)
		while (*sample != '\0')
		{
			if(mode == GetKeyToShow)//1��ֵ 2��ֵת�� 3��ֵ��ʾ
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
//��ȡ�����ļ�¼�����У��̶��ִʺ��ƫ����
static UInt16 GetOffsetAfterStaticWord(Char *content, UInt16 offset)
{
	UInt16		i = 0;
	
	while ((UInt8)content[i] > 0x01)
	{
		i ++;
		if (content[i] == 0x02) //������һ���̶���
		{
			i ++;
			offset += i; //����ƫ����
			content += i; //����ָ��
			i = 0;
		}
	}
	
	return offset;
}
//--------------------------------------------------------------------------
//�����������ļ�¼����ڵ��Ƿ����
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
//���ݽ�����浽��ҳ�Ŀ�ʼλ��
static void RollBackResult(stru_Globe *globe)
{
	UInt8		i;
	UInt8		mask;
	
	//ҳ����һ
	globe->page_count --;
	//���ݵ�ǰҳ�����һ�����
	globe->result = (stru_Result *)globe->result->prev;
	//ѭ����������ǰҳ�ĵ�һ�����
	i = globe->result_status[globe->page_count];
	mask = slot5;
	while (i != slot1)
	{
		if (i & mask) //��λ�ô��ڽ��
		{
			globe->result = (stru_Result *)globe->result->prev;
			i &= (~mask);
		}
		mask = (mask >> 1);
	}
	//������ҳ��־
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
//����������
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
//�۰�������������ؽ����ƫ����
static UInt16 GetContentOffsetFormIndex(Char *key, Char *index, UInt16 index_size)
{
	UInt16			index_index;
	UInt16			index_offset;
	UInt16			index_min = 0;
	UInt16			index_max;
	UInt16			content_offset = 0;
	Int16			i;
	
	index_max = (index_size >> 2) - 1; //����������±�
	if (MemCmp(key, index, 2) >= 0 && MemCmp(key, (index + (index_max << 2)), 2) <= 0) //�������ܴ���
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
//����ѭ������ڵ��е�ƫ��������
static void BuildContentOffsetChain(stru_MBRecord *mb_record, Char *key, Char *index, UInt16 index_size, stru_MBInfo *mb_info)
{
	UInt16				content_offset;
	Char				i;
	Char				j;
	Char				tmp_key[3];
	stru_ContentOffset	*offset;
	
	offset = &mb_record->offset_head;
	MemMove(tmp_key, key, 3);
	//ѭ���������������ܼ�������ƫ��������
	for(i = 'a'; i <= 'z'; i ++)
	{
		if (i != mb_info->wild_char)
		{
			if (key[1] == mb_info->wild_char) //�ڶ����ַ������ܼ���ѭ��ȡֵ
			{
				tmp_key[1] = i;
			}
			else //�������ܼ����޸�i='z'��ʹѭ��ֻ����һ�ξ��˳�
			{
				i = 'z';
			}
			for (j = 'a'; j <= 'z'; j ++)
			{
				if (j != mb_info->wild_char)
				{
					if (key[0] == mb_info->wild_char) //��һ���ַ������ܼ���ѭ��ȡֵ
					{
						tmp_key[0] = j;
					}
					else //�������ܼ����޸�j='z'��ʹѭ��ֻ����һ�ξ��˳�
					{
						j = 'z';
					}
					//��ȡƫ����
					content_offset = GetContentOffsetFormIndex(tmp_key, index, index_size);
					//��¼����������
					if (content_offset > 0)
					{
						//�½��ڵ�
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
//�����Ƿ����� ��������ȫƴ
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
//����ƥ��
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
	return key.length==key_length;//��ȫƥ��
}
//--------------------------------------------------------------------------
//�Ӽ�¼ѭ�������л�ȡ���������ȷ�������ܼ����������ӶൽС�Լ�������С������������һ��ʹ�ļ�¼ѭ������ϲ�������
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
	
	//���潥�����ҵ�����
	org_gradually_search = mb_info->gradually_search;
	if (mb_info->type == 0 && !mb_info->gradually_search && NoVowel(globe->key_buf.key[0]))
	{ //����������������ҹرգ��������ڲ�����������ʱ�򿪽�������
		mb_info->gradually_search = true;
	}
	
	//��¼��ǰ�Ľ���ڵ�
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
	//ѭ��ȡ���
	while (result_count < 5 && more_result_exist)
	{
		//ѭ����¼���������н����ȡ���Ҹý���������ڴ�����һ�µĽڵ㣬���û�������Ľڵ㣬�ݼ��ڴ����Ȳ���������
		mb_record_not_found = true;
		while (mb_record_not_found && more_result_exist)
		{
			more_result_exist = false; //û���κνڵ��ȡ�ı�־
			mb_record_start = globe->mb_record; //��ʼ�ڵ�
			//�ӵ�ǰ�ڵ㿪ʼѭ��һ�ܣ�����һ�����ʵĽڵ�
			do
			{
				if (globe->mb_record->last_word_length == globe->current_word_length && globe->mb_record->more_result_exist)
				{ //�ҵ����ʵĽڵ�
					mb_record_not_found = false;
					more_result_exist = true;
				}
				else //�ýڵ㲻����
				{
					//����һ���ڵ�
					globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
				}
			}while (globe->mb_record != mb_record_start && mb_record_not_found); //��δѭ��һ�ܣ�����δ�ҵ����
			//��û���ҵ����ʵĽڵ㣬����������͵ȵ�����¼����
			if (mb_info->type == 0)
			{
				//��������룬��û���ҵ��κ����ڴ�����һ�µĽڵ㣬�ݼ��ڴ����ȣ�ֱ����������ȡ������Ľڵ�
				while ((! more_result_exist) && globe->current_word_length > 1)
				{
					//�ڴ����ȼ�һ
					globe->current_word_length --;
					//�ӿ�ʼ�ڵ����ѭ��
					globe->mb_record = globe->mb_record_head;
					do
					{
						//δ�����εĽڵ�
						if (globe->mb_record->last_word_length != 0)
						{
							//�����ڵ㳤��
							globe->mb_record->last_word_length = globe->current_word_length;
							//�޸�����
							if (globe->current_word_length < 4)
							{
								globe->mb_record->index[globe->current_word_length] = '\0';
								i = globe->current_word_length; //�Ƚ�����ʱ�ıȽϳ���
							}
							else
							{
								i = 4;
							}
							//��鲢�����ظ��Ľڵ�
							mb_record_start = globe->mb_record;
							mb_record_filter = (stru_MBRecord *)globe->mb_record->next;
							while (mb_record_filter != mb_record_start)
							{
								if (StrNCompare(mb_record_filter->index, mb_record_start->index, 4) == 0)
								{ //�ظ�
									mb_record_filter->last_word_length = 0;
									mb_record_filter->more_result_exist = false;
								}
								mb_record_filter = (stru_MBRecord *)mb_record_filter->next;
							}
							//��������ֻ��һ��������Ҫ���»�ȡ��¼��
							if (globe->current_word_length == 1)
							{
								globe->mb_record->record_index = GetRecordIndex(globe->mb_record->index);
							}
							//���¹���ڵ��ƫ��������
							//ɾ��������
							content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
							while (content_offset_unit != &globe->mb_record->offset_tail)
							{
								pointer_to_delete = (void *)content_offset_unit;
								content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
								MemPtrFree(pointer_to_delete);
							}
							globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
							//����������
							DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);
							record = (Char *)MemHandleLock(record_handle);
							BuildContentOffsetChain(globe->mb_record, (globe->mb_record->index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
							DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
							if (globe->mb_record->offset_head.next == (void *)&globe->mb_record->offset_tail) //û���ҵ�
							{ //�����ýڵ�
								globe->mb_record->more_result_exist = false;
							}
							else
							{
								globe->mb_record->more_result_exist = true;
								more_result_exist = true;
							}
						}
						//�ƶ�����һ���ڵ�
						globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
					}while (globe->mb_record != globe->mb_record_head);
				}
			}
			else if (mb_info->gradually_search && (globe->key_buf.key[0].content[0] != mb_info->wild_char && globe->key_buf.key[0].content[1] != mb_info->wild_char && globe->key_buf.key[0].content[2] != mb_info->wild_char && globe->key_buf.key[0].content[3] != mb_info->wild_char))
			{
				//������룬��û���ҵ��κ����ڴ�����һ�µĽڵ㣬�����ڴ����ȣ�ֱ�����ȵ���4
				while ((! more_result_exist) && (globe->current_word_length < mb_info->key_length/*����*/))
				{
					//�ڴ����ȼ�һ
					globe->current_word_length ++;
					if (globe->current_word_length == 2) //��һ�����������룬��չ��¼����ڵ㣬���¼����¼�ţ���ȡ����ƫ����
					{
						//�ӿ�ʼ�ڵ����ѭ��
						globe->mb_record = globe->mb_record_head;
						do
						{
							if (globe->mb_record->last_word_length < 2) //δ��չ�ļ�¼
							{
								//������չ��ǰ��¼
								//����������
								globe->mb_record->index[1] = 'a';
								globe->mb_record->last_word_length ++;
								//��ȡ�¼�¼��
								globe->mb_record->record_index = GetRecordIndex(globe->mb_record->index);
								//���¹���ڵ��ƫ��������
								//ɾ��������
								content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
								while (content_offset_unit != &globe->mb_record->offset_tail)
								{
									pointer_to_delete = (void *)content_offset_unit;
									content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
									MemPtrFree(pointer_to_delete);
								}
								globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
								//���ü�¼�Ƿ�������
								DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);
								record = MemHandleLock(record_handle);
								if (*((UInt16 *)record) > 0) //������
								{
									//����������
									BuildContentOffsetChain(globe->mb_record, (globe->mb_record->index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
									globe->mb_record->more_result_exist = true;
									more_result_exist = true;
								}
								DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
								//����������¼
								for (new_key = 'b'; new_key <= 'z'; new_key ++)
								{
									if (new_key != mb_info->wild_char)
									{
										//�½���¼
										NewMBRecord(globe);
										StrCopy(globe->mb_record->index, ((stru_MBRecord *)globe->mb_record->prev)->index);
										globe->mb_record->index[1] = new_key;
										globe->mb_record->last_word_length = 2;
										globe->mb_record->record_index = GetRecordIndex(globe->mb_record->index);
										globe->mb_record->offset_head.next = (void *)&globe->mb_record->offset_tail;
										//���ü�¼�Ƿ�������
										DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);										
										record = MemHandleLock(record_handle);
										if (*((UInt16 *)record) > 0) //������
										{
											//����������
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
					else //��������������룬�ؽ�ƫ��������
					{
						globe->mb_record = globe->mb_record_head;
						do
						{
							
							if(globe->current_word_length>3 && mb_info->key_length>4)/*����*/
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
							//���¹���ڵ��ƫ��������
							//ɾ��������
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
								//����������
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
		//�ҵ�һ���ڵ�ȡ���
		if (mb_record_not_found == false && more_result_exist == true)
		{
			//ȡ��¼
			//record_handle = DmGetRecordFromCardAndRAM(globe, globe->mb_record->record_index);
			DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, globe->mb_record->record_index, &record_handle);			
			//������ָ��
			record = (Char *)MemHandleLock(record_handle);
			//һ��ȡ��һ���ڵ��ȫ������ƫ�����еĽ��
			content_offset_unit = (stru_ContentOffset *)globe->mb_record->offset_head.next;
			globe->mb_record->more_result_exist = false; //Ԥ�ñ�־
			//ѭ���ڵ��ÿ��ƫ����
			while (content_offset_unit != &globe->mb_record->offset_tail)
			{
				//ƫ�������
				content = record + content_offset_unit->offset;
				if (mb_info->type == 0) //���������
				{
					//���������ݶ��м������Ϲؼ��ֵ�һ�����
					not_matched = true;
					while (*content != '\0' && not_matched)
					{
						//��¼��ǰƫ����
						tmp = content;
						//��ȡ��ֵ�����ݵĳ��ȣ�ͬʱ�жϵ�ǰ����ı������Ƿ������Ҫ
						if (GetLengthOfResultKey(globe->settingP->filterGB, globe->settingP->filterChar, content, &key_length, &content_length) == globe->current_word_length)
						{
							//ȡ��һ����
							i = 0;
							do
							{
								globe->cache[i] = *tmp;
								tmp ++;
								i ++;
							}while ((UInt8)(*tmp) <= 0x7F && (UInt8)(*tmp) > 0x20 && *tmp != '\'');
							//ƥ����룬���е�һ��������ģ����
							blur_key_index = 0;
							while (not_matched && blur_key_index < 4 && globe->blur_key[blur_key_index].content[0] != '\0')
							{
								if ((StrNCompare(globe->cache, globe->blur_key[blur_key_index].content, globe->blur_key[blur_key_index].length) == 0 && (mb_info->gradually_search)) || //��������
									(StrCompare(globe->cache, globe->blur_key[blur_key_index].content) == 0 && (! mb_info->gradually_search))) //��׼����
								{ //��һ�����ƥ���ˣ����к��������ƥ��
									MemSet(globe->cache, 100, 0x00);
									not_matched = false; //Ԥ��ƥ���־
									j = globe->created_key + 1; //ָ���һ�����֮��
									//tmp ++; //����ָ��ָ���һ�����֮��
									while ((UInt8)(*tmp) <= 0x7F && (UInt8)(*tmp) > 0x20 && (! not_matched)) //δ���ﺺ�֣���δ������ƥ������
									{
										tmp ++; //����ָ��ָ��һ�����֮��
										//����һ�����
										i = 0;
										while ((UInt8)(*tmp) <= 0x7F && (UInt8)(*tmp) > 0x20 && *tmp != '\'')
										{
											globe->cache[i] = *tmp;
											tmp ++;
											i ++;
										}
										//�Ƚϱ���
										if ((StrNCompare(globe->cache, globe->key_buf.key[j].content, globe->key_buf.key[j].length) == 0 && (mb_info->gradually_search)) || //��������
											(StrCompare(globe->cache, globe->key_buf.key[j].content) == 0 && (! mb_info->gradually_search))) //��׼����
										{ //ƥ��
											j ++;
											//tmp ++; //�ƶ�����һ�����룬����Ѿ��Ǻ��֣��򱾲����ᵼ��tmpָ��һ�����ֵĵ�λ
										}
										else //��ƥ��
										{
											not_matched = true; //���ñ�־��ʹѭ���˳�
										}
										MemSet(globe->cache, 100, 0x00);
									}
								}
								blur_key_index ++;
							}
							MemSet(globe->cache, 100, 0x00);
							if (! not_matched ) //�ҵ���ƥ��Ľ��
							{
								NewResult(globe); //�½��������ڵ�
								if (content_length == 0) //Ӣ�Ĵʱ�
								{
									content_length = key_length+1;
									globe->result->result = MemPtrNew(content_length  + 1); //����
									MemSet(globe->result->result, content_length  + 1, 0x00);									
									globe->result->result[0] = chrSpace;
									StrNCopy(globe->result->result + 1, content, content_length-1 );
									globe->result->length = content_length; //����							
								}
								else
								{	
									globe->result->result = MemPtrNew(content_length + 1); //����
									MemSet(globe->result->result, content_length + 1, 0x00);
									StrNCopy(globe->result->result, (content + key_length), content_length);
									globe->result->length = content_length; //����
								}
								globe->result->record_index = globe->mb_record->record_index; //���ڼ�¼
								StrCopy(globe->result->index, globe->mb_record->index); //��ֵ
								if (*(content + key_length + content_length) == '\2') //�̶��ִʱ�־
								{
									globe->result->is_static = true;
								}
								else
								{
									globe->result->is_static = false;
								}
								globe->result->offset = content_offset_unit->offset; //ƫ����
								//���������һ
								result_count ++;
							}
						}
						//ƫ��������һ����������ƥ��ʧ�ܣ����Դ�ƫ����������ƥ��
						content_offset_unit->offset += (key_length + content_length + 1);
						content += (key_length + content_length + 1);
						if (*content != '\0') //������һ�����
						{
							globe->mb_record->more_result_exist = true; //���ñ�־
						}
					}
				}
				else //�������
				{
					if (*content != '\0') //�н��
					{
						//��ȡ��ֵ�����ݵĳ���
						if(GetLengthOfResultKey(globe->settingP->filterGB, globe->settingP->filterChar, content, &key_length, &content_length) && (mb_info->key_length<=4 || globe->key_buf.key[0].length<4 || IsMatched(globe->key_buf.key[0], content, key_length, mb_info))/*����*/)
						{
							NewResult(globe); //�½��������ڵ�							
							globe->result->key = MemPtrNew(key_length + 1);//��ǰ����
							MemSet(globe->result->key, key_length + 1, 0x00);
							StrNCopy(globe->result->key, content, key_length);
							content += key_length;
							globe->result->result = MemPtrNew(content_length + 1); //��ǰ����
							MemSet(globe->result->result, content_length + 1, 0x00);
							StrNCopy(globe->result->result, content, content_length);
							globe->result->length = content_length; //����
							globe->result->record_index = globe->mb_record->record_index; //���ڼ�¼
							StrNCopy(globe->result->index, globe->mb_record->index, 2); //��ֵ
							MemMove((globe->result->index + 2), &content_offset_unit->key, 2);
							globe->result->is_static = (*(content + content_length) == '\2'); //�̶��ִʱ�־
							globe->result->offset = content_offset_unit->offset; //ƫ����
							//ƫ��������һ�����
							/*content_offset_unit->offset += (key_length + content_length + 1);
							if (*(content + content_length) != '\0') //������һ�����
							{
								globe->mb_record->more_result_exist = true; //���ñ�־
							}*/
							//���������һ
							result_count ++;
						}
						else
							content += key_length;
						//ƫ��������һ����������ƥ��ʧ�ܣ����Դ�ƫ����������ƥ��
						content_offset_unit->offset += (key_length + content_length + 1);
						if (*(content + content_length) != '\0') //������һ�����
						{
							globe->mb_record->more_result_exist = true; //���ñ�־
						}
					}
				}
				//����һ����ֵ����
				content_offset_unit = (stru_ContentOffset *)content_offset_unit->next;
			}
			//�������
			DmReleaseRecordFromCardAndRAM(globe->db_ref, globe->mb_record->record_index, &record_handle);
			//�ƶ�����һ���ڵ�
			globe->mb_record = (stru_MBRecord *)globe->mb_record->next;
		}
	}
	//ָ���½�����ĵ�һ���ڵ�
	if (result_count > 0) //�ҵ��µ�����
	{
		globe->result = (stru_Result *)result_last->next;
		globe->no_next = false;
	}
	else //û���ҵ��κ�����
	{
		globe->result = &globe->result_tail;
		globe->no_next = true;
	}
	mb_info->gradually_search = org_gradually_search;
}
//--------------------------------------------------------------------------
//��������
static void BuildIndex(Char *index, UInt16 key_index, UInt16 key_count, UInt16 blur_key_nums, stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16	index_index = 0;
	
	//���������
	MemSet(index, 5, 0x00);
	//��������
	if (mb_info->type == 0) //����������ȴ���ģ����
	{
		//����������
		key_count += key_index;
		if (mb_info->smart_offset > 0) //����ģ��������������һ��ȡģ����
		{
			index[0] = globe->blur_key[blur_key_nums].content[0];
			key_index ++;
			index_index ++;
		}
		//ѭ����������
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
	else //�������
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
		if(key_count>3 && key_count<mb_info->key_length && mb_info->key_length>4 && index[3]!=mb_info->wild_char && mb_info->gradually_search)//����
			index[3]='\0';
	}
}
//--------------------------------------------------------------------------
//���ɵ�һ���ؼ��ֵ�ģ����
static void BuildBlurKey(stru_KeyBufUnit *blur_key, stru_KeyBufUnit *org_key, stru_MBInfo *mb_info)
{
	Char		*tmp;
	UInt16		blur_length;
	UInt16		i = 0;
	
	//���ģ��������
	MemSet(blur_key, 510, 0x00);
	//�����һ����
	StrCopy(blur_key[0].content, org_key->content);
	blur_key[0].length = org_key->length;
	if (mb_info->smart_offset > 0) //����ģ���������й���
	{
		//����ǰģ����
		while (mb_info->blur_tail[i].key1[0] != '\0')
		{
			blur_length = StrLen(mb_info->blur_tail[i].key1); //ȡģ��������
			//ƥ�䲢����ģ����
			if (org_key->length >= blur_length)
			{
				tmp = blur_key[0].content + (org_key->length - blur_length);
				if (StrCompare(tmp, mb_info->blur_tail[i].key1) == 0) //ƥ��
				{
					//�����¼�
					StrCopy(tmp, mb_info->blur_tail[i].key2);
					blur_key[0].length = StrLen(blur_key[0].content);
					StrCopy(blur_key[1].content, org_key->content);
					blur_key[1].length = org_key->length;
					//����
					break;
				}
				else //��ƥ�䣬���Ը�ģ������key2
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
		//�����ģ����
		i = 0;
		while (mb_info->blur_head[i].key1[0] != '\0')
		{
			blur_length = StrLen(mb_info->blur_head[i].key1); //ȡģ��������
			//ƥ�䲢����ģ����
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
//���������ȷ����ģ���������ܼ������ɼ�¼ѭ������ÿ���ڵ��ƫ��������Ȼ���ȡ�������
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
	
	
	//���潥�����ҵ�����
	org_gradually_search = mb_info->gradually_search;
	//��ʼ���ң�����ǲ���������������ҹرա���û���κ�ƥ��Ľ������򿪽������Ҽ���һ��
	do
	{
		//��ʼ����¼����
		InitMBRecord(globe);
		//��ʼ���������
		InitResult(globe);
		if (mb_info->type == 0 && globe->created_key < 10) //���������
		{
			if (globe->key_buf.key[globe->created_key].length > 0)
			{
				//�����һ���ؼ��ֵ�ģ����
				BuildBlurKey(globe->blur_key, &globe->key_buf.key[globe->created_key], mb_info);
				globe->current_word_length = globe->key_buf.key_index + 1 - globe->created_key;
				//ѭ��ȡ�ʳ���ֱ������Ϊ0��ȡ��ӽ��ڴ����ȵĽ��
				while (globe->current_word_length > 0 && globe->mb_record_head == NULL)
				{
					//ѭ��ģ�������������
					blur_key_nums = 0;
					while (blur_key_nums < 4 && globe->blur_key[blur_key_nums].content[0] != '\0')
					{
						//��������
						BuildIndex(index, globe->created_key, globe->current_word_length, blur_key_nums, globe, mb_info);
						if (MBRecordNotExist(index, globe, mb_info))
						{
							//��ȡ��¼��
							record_index = GetRecordIndex(index);
							//���ü�¼�Ƿ�������
							//record_handle = DmGetRecordFromCardAndRAM(globe, record_index);
							DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, record_index, &record_handle);							
							record = (Char *)MemHandleLock(record_handle);
							if ((*(UInt16 *)record) > 0) //������
							{
								//���ɸü�¼������ڵ�
								NewMBRecord(globe);
								globe->mb_record->record_index = record_index; //��¼��
								StrCopy(globe->mb_record->index, index); //����
								globe->mb_record->last_word_length = globe->current_word_length; //�������
								globe->mb_record->more_result_exist = true;
								//ȡ���Ϲؼ��ֵ�ƫ��������
								BuildContentOffsetChain(globe->mb_record, (index + 2), (record + (*(UInt16 *)record)), MemHandleSize(record_handle) - (*(UInt16 *)record), mb_info);
								if (globe->mb_record->offset_head.next == (void *)&globe->mb_record->offset_tail) //û�з��Ϲؼ���Ҫ��Ľ��
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
		else if (globe->key_buf.key[0].length > 0) //�������
		{
			//�볤
			globe->current_word_length = globe->key_buf.key[0].length;
			//��������
			BuildIndex(index, 0, globe->current_word_length, 0, globe, mb_info);
			MemMove(tmp_index, index, 5);
			//ѭ�������������������ڵ��ڶ���ֻ����һ�Σ��������Ϊһ�ҵ�һ�μ���û�н����
			//�ѵڶ�������Ϊ���ܼ�������ڶ���
			do
			{
				//ѭ�����ܼ����ܵķ�Χ������ƥ��ļ�¼
				for(i = 'a'; i <= 'z'; i ++)
				{
					if (i != mb_info->wild_char)
					{
						if (tmp_index[1] == mb_info->wild_char) //�ڶ����ַ������ܼ���ѭ��ȡֵ
						{
							index[1] = i;
						}
						else //�������ܼ����޸�i='z'��ʹѭ��ֻ����һ�ξ��˳�
						{
							i = 'z';
						}
						for (j = 'a'; j <= 'z'; j ++)
						{
							if (j != mb_info->wild_char)
							{
								if (tmp_index[0] == mb_info->wild_char) //��һ���ַ������ܼ���ѭ��ȡֵ
								{
									index[0] = j;
								}
								else //�������ܼ����޸�j='z'��ʹѭ��ֻ����һ�ξ��˳�
								{
									j = 'z';
								}
								if (MBRecordNotExist(index, globe, mb_info))
								{
									//��ȡ��¼��
									record_index = GetRecordIndex(index);
									//���ü�¼�Ƿ�������
									//record_handle = DmGetRecordFromCardAndRAM(globe, record_index);
									DmGetRecordFromCardAndRAM(globe->db_ref, globe->db_file_ref, record_index, &record_handle);									
									record = (Char *)MemHandleLock(record_handle);
									if ((*(UInt16 *)record) > 0) //������
									{
										//���ɸü�¼������ڵ�
										NewMBRecord(globe);
										globe->mb_record->record_index = record_index; //��¼��
										StrCopy(globe->mb_record->index, index); //����
										globe->mb_record->last_word_length = globe->current_word_length; //�������
										globe->mb_record->more_result_exist = true;
										//ȡ���Ϲؼ��ֵ�ƫ��������
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
					globe->current_word_length = 2; //��������϶�ԭ���ĳ���Ϊ1
					second_search = true;
				}
				else
				{
					second_search = false;
				}
			}while (second_search);
			globe->current_word_length --;
		}
		//����ȫ������
		globe->no_prev = true;
		globe->page_count = 0;
		MemSet(globe->result_status, 100, 0x00);
		if (globe->mb_record_head != NULL)
		{
			globe->current_word_length ++;
			//��ȡ���
			GetResultFromMBRecord(globe, mb_info);
		}
		else
		{
			globe->no_next = true;
		}
	
		//...
		if (!mb_info->gradually_search  && (! gradually_researched) && (globe->result == &globe->result_tail || ((globe->result->length>>1) < globe->key_buf.key_index+1)/*����ȫƥ��*/))
		{ //���ܽ������������ҹر�ʱ������ȫƥ����ʱ���ٲ���һ��
			mb_info->gradually_search = true;
			gradually_researched = true;
		}
		else
			gradually_researched = false;
	}while (gradually_researched);
	//�ָ��������ҵ�����
	mb_info->gradually_search = org_gradually_search;
}
//--------------------------------------------------------------------------
//����Ƿ�ֻ���ҽ���һ�����
static Boolean HasOnlyOneResult(stru_Globe *globe)
{
	stru_Result		*result;
	
	if (globe->result_head.next != (void *)&globe->result_tail) //�н��
	{
		result = (stru_Result *)globe->result_head.next; //ָ���һ�����
		if (result->next == (void *)&globe->result_tail) //��һ���������һ��������Ǳ�β
		{
			return true;
		}
	}
	
	return false;
}
#pragma mark -
//--------------------------------------------------------------------------
//���ؼ������Ƿ�������ܼ������з�����
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
//���ƴ�ѡ��
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
	
	if (globe->page_count < 100 && globe->result_head.next != (void *)&globe->result_tail) //�н����δ�ﵽ100ҳ
	{
		font = FntSetFont(pref->displayFont);
		is_small = (pref->displayFont == stdFont) || (pref->displayFont == boldFont);
		is_grad = (pref->curMBInfo.type == 1) && (pref->curMBInfo.gradually_search);
		globe->result_status[globe->page_count] = 0;
		while (globe->result != &globe->result_tail && i < 5 && shouldShowFiveResult)
		{
			//������λ��
			width = FntCharsWidth(globe->result->result, globe->result->length);
			if (globe->result->length > 8)	//�������֣�����ʾһ�����
			{
				if (i == 0)		//�ǵ�һ����������ʾ
				{
					lineCharsCount = globe->result->length;
					y = 16 + (globe->resultRect[0].extent.y - globe->curCharHeight) / 2;
					x = 77 - width / 2;
					shouldShowFiveResult = false;
				}
				else	//���ǣ��˳�
				{
					break;
				}
			}
			else	//�������ڣ�һ����ʾ5�����
			{
				if (is_small && globe->result->length < 8 && globe->result->result[0]!=chrSpace)	//��׼��������
				{
					lineCharsCount = 6;	//��������
				}
				else
				{
					lineCharsCount = 4;	//��������
				}
				if (globe->result->length <= lineCharsCount )
				{
					y = 16 + (globe->resultRect[0].extent.y - globe->curCharHeight) / 2;
					x = globe->resultRect[i].topLeft.x + 15 - width / 2;
					lineCharsCount = globe->result->length;
				}
				else	//��Ҫ������ʾ
				{
					y = 16 + globe->resultRect[0].extent.y / 2 - globe->curCharHeight;
					x = globe->resultRect[i].topLeft.x + 15 - FntCharsWidth(globe->result->result, 4) / 2;
				}
			}
			
			//��ʾ������ʾ
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
			
			//��ʾһ�����
			if (move_bit == globe->cursor)	//���ڹ��λ�ã�������ʾ
			{
				WinSetTextColorRGB(&pref->resultHighlightForeColor, &foreColor);
				WinSetBackColorRGB(&pref->resultHighlightBackColor, &backColor);
			}
			else	//�����ȵ�
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
				if (globe->result->length > lineCharsCount)	//��Ҫ������ʾ
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
			else//Ӣ�����
			{
				if (globe->result->length > lineCharsCount + 1)	//��Ҫ������ʾ
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
			
			//��ʾ�̶����
			if (globe->result->is_static)
			{
				y += globe->curCharHeight;
				WinDrawLine(x, y, x + globe->curCharWidth * (lineCharsCount / 2) - 1, y);
			}
			//��¼��λ��
			globe->result_status[globe->page_count] |= (slot << move_bit);
			move_bit ++;
			i ++;
			
			//ȡ��һ�����
			globe->result = (stru_Result *)globe->result->next; //�ƶ�����һ��
			if (globe->result == &globe->result_tail) //�������Ľ�β��ȡ�µĽ��
			{
				GetResultFromMBRecord(globe, &pref->curMBInfo);
			}
		}
		//ҳ������
		globe->page_count ++;
		//��ҳ��־
		globe->no_next = (globe->result == &globe->result_tail); //û����һ�������
		FntSetFont(font);
	}
}
//--------------------------------------------------------------------------
//���ƹؼ���
static void DrawKey(stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16	i = 0;
	UInt16	j = 0;
	Int16	k;
	UInt16	key_length;
	Char	*tmp;
	FontID	font;
	
	if (globe->in_create_word_mode) //�����ģʽ
	{
		//��������ɵ������
		for (i = 0; i < globe->created_word_count; i ++)
		{
			StrCat(globe->cache, globe->created_word[i].result);
		}
		i = globe->created_key;
	}
	if (globe->english_mode)
	{
		//Ӣ��ģʽ��ֱ�Ӱѹؼ�����ӡ����
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
		//����ؼ��ִ����������ͨģʽ����ӵ�һ���ؼ��ֿ�ʼ�������δ�����ʵĹؼ��ֿ�ʼ
		if (i <= globe->key_buf.key_index)
		{
			if (mb_info->translate_offset > 0) //����м�ֵת��
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
			else //����Ҫ���м�ֵת��
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
		//����
		key_length = StrLen(globe->cache);
		//�Ƿ���ʾ��'��
		if (globe->new_key)
		{
			StrCat(globe->cache, "\'");
			key_length ++;
		}
	}
	//��������
	font = FntSetFont(stdFont);
	//���ؼ��ִ�
	k = 77 - FntCharsWidth(globe->cache, key_length) / 2;
	if (k < 35)
	{
		k = 35;
	}
	WinDrawTruncChars(globe->cache, key_length, k, 2, 132 - k);
	//�ָ�����
	FntSetFont(font);
	//��ջ���
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

//���������
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
	
	//�ڻ�ͼ�����л�ͼ
	current_window = WinSetDrawWindow(globe->draw_buf);
	WinEraseWindow();
	
	//��ʾ����
	DrawResult(globe, pref);
	
	//���ؼ�������
	WinSetTextColorRGB(&pref->codeForeColor, &preventTextColor);
	WinSetBackColorRGB(&pref->codeBackColor, &preventBackColor);
	rectangle.topLeft.x = 1;
	rectangle.topLeft.y = 1;
	rectangle.extent.x = 152;
	rectangle.extent.y = 14;
	WinEraseRectangle(&rectangle, 0);
	DrawKey(globe, &pref->curMBInfo);
	if (globe->in_create_word_mode && !pref->menu_button) //�����
	{
		strID = StrWord;
	}
	else if (globe->english_mode && !pref->menu_button) //Ӣ��ģʽ
	{
		strID = StrEng;
	}
	else if(pref->activeStatus & tempMBSwitchMask)//��ʱ���ģʽ
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
	else if(pref->menu_button)//�������
	{
		CtlSetLabel (FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrMENU)),pref->curMBInfo.name);
		//CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrMENU)));
	}
	else //�������
	{
		WinDrawChars(pref->curMBInfo.name, StrLen(pref->curMBInfo.name), 2, 2);
	}
	
	//���·�ҳ��־
	/*if (!pref->choice_button)//��ҳ��ť
	{
	font_id = FntSetFont(symbol7Font); //��������
	WinDrawChar((globe->no_prev?0x0003:0x0001), 144, 0); //�һ��ɫ�ϼ�ͷ
	WinDrawChar((globe->no_next?0x0004:0x0002), 144, 7); //�һ��ɫ�¼�ͷ
	FntSetFont(font_id);
	}*/
	
	//�ָ���ɫ
	WinSetTextColorRGB(&preventTextColor, NULL);
	WinSetBackColorRGB(&preventBackColor, NULL);
	
	//������ͼ����
	WinSetDrawWindow(current_window); //�ָ���ͼ����
	
	if (isGrfLocked(pref))
	{
		rectangle.topLeft.x = 132;
		rectangle.topLeft.y = 2;
		rectangle.extent.x = 10;
		rectangle.extent.y = 10;
		gsi_save = WinSaveBits(&rectangle, &error); //����gsiָʾ��
		WinCopyRectangle(globe->draw_buf, current_window, form_rect, 1, 1, winPaint); //�ѻ��濽��������
		WinRestoreBits(gsi_save, 132, 2); //�ָ�gsiָʾ��
	}
	else
	{	
		WinCopyRectangle(globe->draw_buf, current_window, form_rect, 1, 1, winPaint); //�ѻ��濽��������
	}
	
	//WinCopyRectangle(globe->draw_buf, current_window, form_rect, 1, 1, winPaint); //�ѻ��濽��������
	//�ָ������ť
	
	if (pref->choice_button)//��ҳ��ť
	{
		CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrUP)));
		CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrDOWN)));
	}
	else//���·�ҳ��־
	{
		font_id = FntSetFont(symbol7Font); //��������
		WinDrawChar((globe->no_prev?0x0003:0x0001), 144, 0); //�һ��ɫ�ϼ�ͷ
		WinDrawChar((globe->no_next?0x0004:0x0002), 144, 7); //�һ��ɫ�¼�ͷ
		FntSetFont(font_id);
	}
	/*if (pref->menu_button)//�˵���ť
	{
		CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrMENU)));
	}*/
	//if (StrCaselessCompare(pref->curMBInfo.file_name + (StrLen(pref->curMBInfo.file_name) - 3), "GBK")==0)//�ַ���
	//	CtlDrawControl(FrmGetObjectPtr(form, FrmGetObjectIndex(form,btnChrGBK)));
	//����߿�
	//WinSetForeColorRGB(&pref->frameColor, &preventTextColor);
	//WinDrawRectangleFrame(popupFrame, form_rect);
	WinSetForeColorRGB(&preventTextColor, NULL);
}
#pragma mark -

//--------------------------------------------------------------------------
//����ѡ�����Ĵ�Ƶλ��
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
	
	//��ȡҪ������Ƶ�Ľ�����ڵļ�¼���Լ�������ڵļ�¼�ε�ƫ����
	record_handle = DmGetRecord(globe->db_ref, result->record_index); //��ȡ��¼
	record = (Char *)MemHandleLock(record_handle);
	offset = GetContentOffsetFormIndex((result->index + 2), (record + (*((UInt16 *)record))), MemHandleSize(record_handle) - (*((UInt16 *)record)));
	//�����̶��ִʣ���ȡ�̶��ִʺ�ĵ�һ�����ݵ�ƫ����
	offset = GetOffsetAfterStaticWord((record + offset), offset);
	if (offset < result->offset) //��Ҫ���д�Ƶ����
	{
		n = StrLen((record + offset)); //�γ���
		//������ǰ���Ĳ���
		if (mode == fixModeNormal) //����ģʽ
		{
			step = ((result->offset - offset) >> 1);
			if (step == 0)
			{
				step = 1;
			}
			//��ǰ΢��������ֱ���ҵ�һ�����������ݵ�Ԫ
			while ((result->offset - step >= offset) && (*(record + (result->offset - step)) != '\1' || step == 1))
			{
				step ++;
			}
			step --;
		}
		else //ǿ���ƶ�����һλ
		{
			step = result->offset - offset;
		}
		//��ȡҪ������Ƶ�Ľ�����������ݵ�Ԫ
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
		//�õ�����Ҫ������Ƶ�Ľ�����ڵ�ƫ�������Ӹ�ƫ������Ҫ������Ƶ�Ľ��֮ǰ�Ķγ���
		x = result->offset - step; //�����λ��
		n = result->offset - x; //���Ƶ����ݵĳ���
		//�Ӳ���ƫ�������Ƽ�¼����
		DmWrite(record, x + StrLen(globe->cache), (record + x), n);
		//д��Ҫ������Ƶ�Ľ��
		DmWrite(record, x, globe->cache, StrLen(globe->cache));
	}
	else if (offset == result->offset && set_static) //�ѵ�һ���������Ϊ�̶�
	{
		while ((UInt8)record[offset] > 0x02)
		{
			offset ++;
		}
		DmSet(record, offset, 1, 0x02);
	}
	//�ͷż�¼
	MemHandleUnlock(record_handle);
	DmReleaseRecord(globe->db_ref, result->record_index, true);
}

//--------------------------------------------------------------------------
//ǿ����ǰѡ���Ĵ��飬����������Ҫ���²�����������棬���򷵻ؼ�
static Boolean MoveWordToTop(stru_Globe *globe, Boolean set_static)
{
	UInt16			i;
	UInt16			selector;
	stru_Result		*result_prev;
	
	if (globe->page_count > 0) //�н��
	{
		//��¼��ǰ���
		result_prev = globe->result;
		//���ݵ�ǰҳ�Ŀ�ͷ
		RollBackResult(globe);
		selector = (slot1 << globe->cursor);
		if ((globe->result_status[globe->page_count] & selector)) //���ѡ��Ľ������
		{
			//�ƶ����û�ѡ��Ľ��
			i = 1;
			while (i != selector)
			{
				globe->result = (stru_Result *)globe->result->next;
				i = (i << 1);
			}
			//ǿ����ǰ���
			FixWordOffset(globe->result, globe, fixModeTop, set_static);
			return true;
		}
		else //�ָ������¼״̬
		{
			globe->result = result_prev;
		}
	}
	return false;
}
//--------------------------------------------------------------------------
//����ִʵĹ̶���
static void UnsetStaticWord(stru_Globe *globe)
{
	UInt16			i;
	UInt16			selector;
	UInt16			content_size = 0;
	UInt16			offset;
	UInt16			move_up_size;
	Char			*record;
	MemHandle		record_handle;
	
	if (globe->page_count > 0) //�н��
	{
		//���ݵ�ǰҳ�Ŀ�ͷ
		RollBackResult(globe);
		selector = (slot1 << globe->cursor);
		if ((globe->result_status[globe->page_count] & selector)) //���ѡ��Ľ������
		{
			//�ƶ����û�ѡ��Ľ��
			i = 1;
			while (i != selector)
			{
				globe->result = (stru_Result *)globe->result->next;
				i = (i << 1);
			}
			if (globe->result->is_static) //�ǹ̶��ִ�
			{
				//��ȡ��¼
				record_handle = DmGetRecord(globe->db_ref, globe->result->record_index);
				record = (Char *)MemHandleLock(record_handle);
				//ƫ�Ƶ���ǰ�̶����
				offset = globe->result->offset;
				//��ȡ�����Ľ������
				while ((UInt8)(record[offset]) > 0x02)
				{
					globe->cache[content_size] = record[offset];
					offset ++;
					content_size ++;
				}
				globe->cache[content_size] = '\1'; //ȡ���̶����
				content_size ++;
				offset ++; //ָ����������
				//ȡ�ǹ̶��ִʵĵ�һ�������ƫ��������ȥ��ǰ�̶������ĵ�һ�������ƫ�������ó�Ҫǰ�Ƶ����ݵĳ���
				move_up_size = GetOffsetAfterStaticWord((record + offset), offset) - offset;
				//ǰ������
				if (move_up_size > 0)
				{
					DmWrite(record, globe->result->offset, (record + offset), move_up_size);
					offset = globe->result->offset + move_up_size;
				}
				else
				{
					offset = globe->result->offset;
				}
				//д�뵱ǰ�̶����ݣ���ȡ���̶���ǣ�
				DmWrite(record, offset, globe->cache, content_size);
				//��ջ���
				MemSet(globe->cache, content_size, 0x00);
				//�ͷż�¼
				MemHandleUnlock(record_handle);
				DmReleaseRecord(globe->db_ref, globe->result->record_index, true);
			}
		}
	}
}
//--------------------------------------------------------------------------
//ɾ��ѡ���Ĵ��飬����������Ҫ���²�����������棬���򷵻ؼ�
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
	
	if (globe->page_count > 0) //�н��
	{
		//��¼��ǰ���
		result_prev = globe->result;
		//���ݵ�ǰҳ�Ŀ�ͷ
		RollBackResult(globe);
		selector = (slot1 << globe->cursor);
		if ((globe->result_status[globe->page_count] & selector)) //���ѡ��Ľ������
		{
			//�ƶ����û�ѡ��Ľ��
			i = 1;
			while (i != selector)
			{
				globe->result = (stru_Result *)globe->result->next;
				i = (i << 1);
			}
			if (globe->result->length > 2) //���ǵ���
			{
				//��ȡ��¼
				record_handle = DmGetRecord(globe->db_ref, globe->result->record_index);
				record_size = MemHandleSize(record_handle);
				record = (Char *)MemHandleLock(record_handle);
				//��ȡ����ƫ����
				index_offset = (*((UInt16 *)record));
				//��ȡ�����εĳ���
				index_size = record_size - index_offset;
				//��ȡҪɾ���Ľ�������ݶεĳ���
				tmp = record + globe->result->offset;
				while ((UInt8)(*tmp) > 0x02)
				{
					content_size ++;
					tmp ++;
				}
				content_size ++;
				//���ұ�ɾ���Ľ������Ӧ���������������϶����ڣ����Բ���Ҫ���ñ߽��жϣ�
				tmp = record + index_offset;
				i = 0;
				while (MemCmp(tmp, (globe->result->index + 2), 2) != 0)
				{
					tmp += 4;
					i += 4;
				}
				//����֮���������ƫ����
				tmp += 4;
				i += 4;
				while (i < index_size)
				{
					MemMove(&index_offset, (tmp + 2), 2); //��ȡƫ����
					index_offset -= content_size; //����
					DmWrite(record, ((*((UInt16 *)record)) + i + 2), &index_offset, 2); //д����ֵ
					tmp += 4;
					i += 4;
				}
				//ǰ�Ƽ�¼���ݣ�����Ҫɾ���ļ�¼
				DmWrite(record, globe->result->offset, (record + (globe->result->offset + content_size)), record_size - globe->result->offset - content_size);
				//�޸�����ƫ����
				index_offset = (*((UInt16 *)record)) - content_size;
				DmWrite(record, 0, &index_offset, 2);
				//�ͷż�¼
				MemHandleUnlock(record_handle);
				DmReleaseRecord(globe->db_ref, globe->result->record_index, true);
				//������¼
				DmResizeRecord(globe->db_ref, globe->result->record_index, record_size - content_size);
			}
			return true;
		}
		else //�ָ������¼״̬
		{
			globe->result = result_prev;
		}
	}
	
	return false;
}
//--------------------------------------------------------------------------
//��������������
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
	
	//�ͷ��ڴ�
	InitResult(globe);
	InitMBRecord(globe);
					
	//---------------------------�����µ�Ԫ--------------------------
	if (mb_info->type == 0) //������������ȹ���ؼ��֣�ͬʱ���������Ĺؼ��ִ������ٹ�������
	{
		//��������
		key_buf = (stru_KeyBuf *)MemPtrNew(sizeof(stru_KeyBuf));
		MemSet(key_buf, sizeof(stru_KeyBuf), 0x00);
		//ѭ�������ֵ
		for (i = 0; i < globe->created_word_count; i ++)
		{
			//��ȡ����
			content_count = (globe->created_word[i].length >> 1);
			//ȡ��¼��ƫ���������б���Ľ����λ��
			record_handle = DmQueryRecord(globe->db_ref, globe->created_word[i].record_index);
			record = (((Char *)MemHandleLock(record_handle)) + globe->created_word[i].offset);
			//ѭ����ȡ��ǰ����ʻ���ļ�ֵ
			for (j = 0; j < content_count; j ++)
			{
				//��ȡ��ֵ
				while (*record != '\'' && ((UInt8)(*record)) < 0x80)
				{
					//��ֵ
					key_buf->key[key_buf->key_index].content[key_buf->key[key_buf->key_index].length] = (*record);
					key_buf->key[key_buf->key_index].length ++;
					//�ؼ��ִ�
					globe->cache[new_word_unit_length] = (*record);
					new_word_unit_length ++;
					record ++;
				}
				//��Ӹ������š�'��
				globe->cache[new_word_unit_length] = '\'';
				record ++;
				new_word_unit_length ++;
				key_buf->key_index ++;
			}
			//�ͷż�¼
			MemHandleUnlock(record_handle);
		}
		key_buf->key_index --;
		//�ؼ��ִ��������һ����'����������
		new_word_unit_length --;
		globe->cache[new_word_unit_length] = '\0';
		//ѭ����������
		j = 0;
		for (i = 0; i <= key_buf->key_index; i ++)
		{
			index[j] = key_buf->key[i].content[0];
			if (j < 3)
			{
				j ++;
			}
		}
		//���������´���
		StrCat(globe->cache, content);
		StrCat(globe->cache, "\1");
		//��ȡ�´�����ܳ��ȣ���������β��0x00��
		new_word_unit_length += StrLen(content) + 1;
	}
	else //�������ֱ�ӹ�������
	{
		//���������´���
		StrCopy(globe->cache, content);
		StrCat(globe->cache, "\1");
		//��ȡ�´�����ܳ��ȣ���������β��0x00��
		new_word_unit_length = StrLen(globe->cache);
	}
	key_index = (index + 2);
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//  ���ν����
	//  globe->cache			- Ҫ������������ݶΣ��������롢���ֺͽ�β��ʶ��0x01
	//  new_word_unit_length	- �������ݶεĳ��ȣ��˳��Ȳ������ַ�����β��0x00
	//  key_index				- �´���ļ�¼������ֵ
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//---------------------------������¼����--------------------------
	//��ȡ�´���ļ�¼��
	record_index = GetRecordIndex(index);
	//ȡ��¼����¼����
	record_handle = DmGetRecord(globe->db_ref, record_index);
	record_size = MemHandleSize(record_handle);
	record = (Char *)MemHandleLock(record_handle);
	//��ȡ����ƫ����
	MemMove(&index_offset, record, 2);
	if (index_offset == 0) //�������һ��û���κ����ݵļ�¼���Ѽ�¼��չ��ӵ��һ�������Ŀռ�¼������ֵΪ��ǰҪ����Ĵʵ�����
	{
		MemHandleUnlock(record_handle); //����
		DmReleaseRecord(globe->db_ref, record_index, false);
		DmResizeRecord(globe->db_ref, record_index, 7); //�����ߴ�
		record_handle = DmGetRecord(globe->db_ref, record_index);
		record = (Char *)MemHandleLock(record_handle);
		DmSet(record, 0, 7, 0x00); //���
		DmSet(record, 1, 1, 0x03); //����ƫ����
		DmWrite(record, 3, key_index, 2); //����ֵ
		DmSet(record, 6, 1, 0x02); //����ƫ����
		index_offset = 3;
		record_size = 7;
	}
	else if (MemCmp((record + (record_size - 4)), key_index, 2) < 0) //��¼������������С���½��Ĵ��������
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
	//  ���ν����
	//  index_offset			- ��¼�����ε�ƫ����
	//  record					- ������ļ�¼�Σ���֤����һ�����ա��ļ�¼
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//---------------------------д���¼------------------------------
	//����������������ƥ��Ľڵ㣬������½ڵ㣬����ȷ���������ڵ������ƫ������������¼�ĳߴ��Լ�������ƫ����
	index_size = record_size - index_offset; //�����γ���
	tmp_index = (record + index_offset); //��ȡ������
	for (read_index_size = 0; read_index_size < index_size; read_index_size += 4)
	{
		memcmp_result = MemCmp(tmp_index, key_index, 2);
		if (memcmp_result == 0) //�ҵ�ƥ��Ľڵ�
		{
			//ƥ��ڵ㣬����������Ӧ�ĳ��ȣ������ڵ�֮�������ƫ����������ƫ��������¼�ߴ�������������ڵ�ߴ�
			//��ȡ��¼ƫ����
			MemMove(&content_offset, (tmp_index + 2), 2);
			//�����ڵ�֮�������ƫ����
			tmp_index += 4;
			read_index_size += 4;
			for (; read_index_size < index_size; read_index_size += 4)
			{
				//��ȡ�ýڵ������ƫ����
				MemMove(&i, (tmp_index + 2), 2);
				//����ƫ����
				i += new_word_unit_length;
				//дȥƫ����
				DmWrite(record, index_offset + read_index_size + 2, &i, 2);
				//�ƶ�����һ�������ڵ�
				tmp_index += 4;
			}
			//������¼����
			MemHandleUnlock(record_handle);
			DmReleaseRecord(globe->db_ref, record_index, true);
			DmResizeRecord(globe->db_ref, record_index, (record_size + new_word_unit_length));
			record_handle = DmGetRecord(globe->db_ref, record_index);
			record = (Char *)MemHandleLock(record_handle);
			//ƫ�������ݶΣ��������̶���
			content_offset += GetOffsetAfterStaticWord((record + content_offset), 0); //�̶���
			tmp_content = (record + content_offset); //��ȡ���ݶ�
			//��������
			DmWrite(record, (content_offset + new_word_unit_length), tmp_content, (record_size - content_offset));
			//��д������
			DmWrite(record, content_offset, globe->cache, new_word_unit_length);
			//������¼�ߴ�
			record_size += new_word_unit_length;
			//��������ƫ����
			index_offset += new_word_unit_length;
			DmWrite(record, 0, &index_offset, 2);
			//����ѭ��
			break;
		}
		else if (memcmp_result > 0) //ƥ��Ľڵ㲻����
		{
			//����һ���սڵ㣬�����½ڵ�֮�������ƫ����������ƫ��������¼�ߴ�����������ڵ�ߴ�
			//���ڵ�ǰ�ڵ��λ�ò����������ڵ㣬�ȶԵ�ǰ�ڵ㼰���Ľڵ��������������
			MemMove(&content_offset, (tmp_index + 2), 2); //��ȡ��ǰ������ָ������ƫ����
			//��¼��ǰ�ڵ��ƫ����
			j = index_offset + read_index_size;
			current_index_size = read_index_size;
			//ѭ��������ǰ��������������������ƫ����
			for ( ; read_index_size < index_size; read_index_size += 4)
			{
				MemMove(&i, (tmp_index + 2), 2); //��ȡ
				i ++; //�սڵ㳤��Ϊ1
				DmWrite(record, index_offset + read_index_size + 2, &i, 2); //д��
				tmp_index += 4;
			}
			//������¼����
			MemHandleUnlock(record_handle);
			DmReleaseRecord(globe->db_ref, record_index, true);
			DmResizeRecord(globe->db_ref, record_index, (record_size + 5)); //������4��������1
			record_handle = DmGetRecord(globe->db_ref, record_index);
			record = (Char *)MemHandleLock(record_handle);
			//������ƫ����λ�ú���1���ֽڣ����������ݽڵ�
			DmWrite(record, content_offset + 1, (record + content_offset), record_size - content_offset);
			DmSet(record, content_offset, 1, 0x00);
			//�ӵ�ǰ�����ڵ�λ�ú���4���ֽڣ������������ڵ�
			j ++; //�����ݽڵ㵼������ƫ����������1
			DmWrite(record, j + 4, (record + j), record_size - j + 1);
			//�ڿ������ڵ���������ݽڵ��ƫ����
			DmWrite(record, j, key_index, 2); //��ֵ
			DmWrite(record, j + 2, &content_offset, 2); //ƫ����
			//������¼�ߴ�
			record_size += 5;
			//��������ƫ����
			index_offset ++;
			DmWrite(record, 0, &index_offset, 2);
			//������ǰ����ָ��
			tmp_index = record + (j - 4); //ָ����һ���ڵ㣬�Ա�����һ��ѭ��ʱָ�򱾽ڵ�
			read_index_size = current_index_size - 4;
			index_size += 4; //����������1
		}
		tmp_index += 4;
	}
	
	//�ͷż�¼
	MemHandleUnlock(record_handle);
	DmReleaseRecord(globe->db_ref, record_index, true);
	//�ͷ��ڴ�
	if (key_buf != NULL)
	{
		MemPtrFree(key_buf);
	}
}

#pragma mark -

//--------------------------------------------------------------------------
//����һ����ӽ��û�ѡ��Ľ����λ��
static Boolean GetNearlySelector(UInt8 *selector, stru_Globe *globe, UInt16 page)
{
	if ((globe->result_status[page] & (*selector))) //ֱ�Ӷ�Ӧ
	{
		return true;
	}
	else if ((*selector) == slot5) //slot5û�н���Ļ�������slot3
	{
		if ((globe->result_status[page] & slot3)) //�н��
		{
			(*selector) = slot3;
			return true;
		}
	}
	else if ((*selector) == slot4) //slot4û�н���Ļ�������slot2
	{
		if ((globe->result_status[page] & slot2)) //�н��
		{
			(*selector) = slot2;
			return true;
		}
	}
	return false;
}
//--------------------------------------------------------------------------
//����ѡ���Ľ��������ȷ��������ʵ�����
static Boolean SelectResult(Char *buf, UInt8 *operation, UInt8 selector, stru_Globe *globe, stru_MBInfo *mb_info, UInt8 mode)
{
	stru_Result		*result;
	UInt8			i;
	Char			index[5];
	
	if (globe->result_head.next != (void *)&globe->result_tail && mode != SelectByEnterKey) //����ѡ�ּ�ѡ�֣����н��
	{
		if (GetNearlySelector(&selector, globe, globe->page_count - 1)) //�û�ѡ���λ���н��
		{
			//��������ǰҳ����ʼ
			RollBackResult(globe);
			//�ƶ����û�ѡ��Ľ��
			result = globe->result;
			i = 1;
			while (i != selector)
			{
				result = (stru_Result *)result->next;
				i = (i << 1);
			}
			//��ȡ���
			if ((mb_info->type == 1 && globe->in_create_word_mode && globe->created_word_count < 10) || (mb_info->type == 0 && (((result->length+1) >> 1) + globe->created_key) <= globe->key_buf.key_index))
			{ //�����ģʽ
				globe->in_create_word_mode = true; //����ʱ�־
				StrCopy(globe->created_word[globe->created_word_count].result, result->result); //���
				globe->created_word[globe->created_word_count].length = result->length; //����
				globe->created_word[globe->created_word_count].record_index = result->record_index; //��¼��
				MemMove(globe->created_word[globe->created_word_count].index, result->index, 5); //��ֵ
				globe->created_word[globe->created_word_count].offset = result->offset; //ƫ����
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
			{ //���ؽ��
				//���������ʹ���
				if (globe->created_word_count < 10)
				{
					StrCopy(globe->created_word[globe->created_word_count].result, result->result); //���
					globe->created_word[globe->created_word_count].length = result->length; //����
					globe->created_word[globe->created_word_count].record_index = result->record_index; //��¼��
					MemMove(globe->created_word[globe->created_word_count].index, result->index, 5); //��ֵ
					globe->created_word[globe->created_word_count].offset = result->offset; //ƫ����
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
				{ //�����������������
					for (i = 0; i < globe->created_key; i ++)
					{
						StrCat(buf, globe->created_word[i].result);
					}
					//��д����������
					if (globe->db_file_ref==NULL)//���ڿ���			
					{
						MemSet(index, 5, 0x00);
						SaveWord(index, buf, globe, mb_info);
					}
				}
				else
				{ //ֱ�ӷ��ؽ��
					if(mode>0)//�Դʶ���
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
					//������Ƶ
					if (mb_info->frequency_adjust && globe->db_file_ref == NULL)
					{
						FixWordOffset(result, globe, fixModeNormal, false);
					}
				}
				//������Ϣ
				(*operation) = pimeExit;
			}
			return true;
		}
	}
	else if (mode == SelectByEnterKey) //�س���ѡ��
	{
		//�����Ѿ���ɵ������
		for (i = 0; i < globe->created_word_count; i ++)
		{
			StrCat(buf, globe->created_word[i].result);
			//������Ϣ
			(*operation) = pimeCreateWord;
		}
		if (globe->key_buf.key[0].length > 0)
		{
			//����δ��ɵĹؼ���
			for (i = globe->created_key; i <= globe->key_buf.key_index; i ++)
			{
				StrCat(buf, globe->key_buf.key[i].content);
				//������Ϣ
				(*operation) = pimeExit;
			}
		}
		return true;
	}
	
	return false;
}
//--------------------------------------------------------------------------
//�ƶ�ѡ�ֹ��
static void MoveResultCursor(stru_Globe *globe, UInt8 op)
{	
	if (globe->page_count > 0) //�н��
	{
		//��������ҳ��һ�����
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
//ת������ֵ
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
//������йؼ��֣�������û�з���Ч�ַ������ؼ٣����򷵻���
static Boolean KeyBufHasUnusedChar(stru_Globe *globe, stru_MBInfo *mb_info)
{
	UInt16		i;
	UInt16		j;
	UInt16		used_char_length;
	
	used_char_length = StrLen(mb_info->used_char);
	if (mb_info->translate_offset > 0) //��Ҫ���м�ֵת�������
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
	//�Ϸ����ж�
	for (i = 0; i <= globe->key_buf.key_index; i ++)
	{
		for (j = 0; j < globe->key_buf.key[i].length; j ++)
		{
			if (StrChr(globe->cache, globe->key_buf.key[i].content[j]) == NULL) //�ҵ�һ������Ч�ַ�
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
//�����Ƿ�ѡ�ּ������ǣ����ض�Ӧ��ѡ��λ�ã����򷵻�0xFF
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
			if ((key == pref->Selector[i] || key == pref->Selector2[i])) //ƥ��
			{
				return (slot1 << i);
			}
		}
	}
	
	return 0xFF;
}
//--------------------------------------------------------------------------
//�Ѱ���������ؼ��ֻ��棬�Ѵ�������
static Boolean KeywordHandler(WChar new_key, UInt8 *operation, stru_Globe *globe, stru_Pref *pref, Char *buf)
{
	UInt16		key_cache_length;
	UInt16		sample_length;
	UInt32		read_size;
	Boolean		matched = false;
	Char		*key_syncopate;
	Char		*tmp = NULL;
	WChar		caseSyncopateKey = pref->SyncopateKey;
	
	if(new_key == pref->SyncopateKey)//�������������½��ؼ��ֱ��
	{
		if (pref->KBMode != KBModeExtFull)
		{
			if (globe->english_mode)
			{
				if (globe->key_buf.key_index <= 9 && globe->key_buf.key[globe->key_buf.key_index].length < 99)
				{ //�����㹻���������
					globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
					globe->key_buf.key[globe->key_buf.key_index].length ++;
				}
			}
			else if (globe->key_buf.key[globe->key_buf.key_index].length != 0 && (! (globe->new_key || (globe->key_buf.key_index == 9 && globe->key_buf.key[9].length == pref->curMBInfo.key_length))) && pref->curMBInfo.type == 0)
			{ //δ���ù�����Ұ�������δ�����ҵ�ǰ����ǿգ���������
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
			case keySemiColon:		//;����'������ȫ����ģʽ���½��ؼ��ֱ��
			case keySingleQuote:
			{
				if (pref->KBMode == KBModeExtFull)
				{
					if (globe->english_mode)
					{
						if (globe->key_buf.key_index <= 9 && globe->key_buf.key[globe->key_buf.key_index].length < 99)
						{ //�����㹻���������
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							globe->key_buf.key[globe->key_buf.key_index].length ++;
						}
					}
					else if (globe->key_buf.key[globe->key_buf.key_index].length != 0 && (! (globe->new_key || (globe->key_buf.key_index == 9 && globe->key_buf.key[9].length == pref->curMBInfo.key_length))) && pref->curMBInfo.type == 0)
					{ //δ���ù�����Ұ�������δ�����ҵ�ǰ����ǿգ���������
						globe->new_key = true;
					}
				}
				else
				{
					return false;
				}
				break;
			}
			case keyBackspace: //�˸����ɾ���½��ؼ��ֱ�ǣ���ӹؼ�����ɾ��һ���ַ�
			{
				if (globe->new_key) //���ù��½��ؼ��ֱ�ǣ�ȡ����
				{
					globe->new_key = false;
				}
				else if ((globe->key_buf.key_index >= 0 && globe->key_buf.key[0].length > 0) || (globe->in_create_word_mode && pref->curMBInfo.type == 1)) //�йؼ������ݣ����Խ���ɾ������
				{
					if (globe->in_create_word_mode) //�����ģʽ
					{
						if (globe->created_word_count > 0)
						{
							//����ʻ��������һ
							globe->created_word_count --;
							//�ָ�����ɵ�����ʼ���
							if (pref->curMBInfo.type == 0)
							{
								globe->created_key -= (globe->created_word[globe->created_word_count].length >> 1);
							}
							//�������ʻ���
							MemSet(globe->created_word[globe->created_word_count].result, 50, 0x00);
							globe->created_word[globe->created_word_count].length = 0;
							globe->created_word[globe->created_word_count].record_index = 0;
							MemSet(globe->created_word[globe->created_word_count].index, 5, 0x00);
							globe->created_word[globe->created_word_count].offset = 0;
							//��û������ɵ�������ˣ��˳����ģʽ
							if (globe->created_word_count == 0)
							{
								globe->in_create_word_mode = false;
							}
						}
						else if (globe->key_buf.key[0].length > 0) //������������ģʽ��ɾ���ؼ���
						{
							globe->key_buf.key[globe->key_buf.key_index].length --;
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = 0x00;
						}
						else //�˳����ģʽ
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
				else //û���κο���ɾ�������ݣ����ء��١�
				{
					(*operation) = pimeExit;
				}
				break;
			}
			default: //�ѺϷ��ļ�ֵ���������
			{
				if (new_key >= 33 && new_key <= 126) //�����ַ�
				{
					if (new_key == keyComma && pref->KBMode == KBModeExtFull)
					{
						return false;
					}else if(pref->extractChar && new_key >=keyOne && new_key <= keyNine)//���ּ� �Դʶ���
					{
						return false;
					}
					if (StrChr(pref->curMBInfo.used_char, new_key) != NULL && (! globe->english_mode)) //����ģʽ���Ϸ���ֵ
					{
						if (globe->new_key || globe->key_buf.key[globe->key_buf.key_index].length == pref->curMBInfo.key_length)
						{ //���½��ؼ��ֱ�ǣ���ǰһ����������
							if (pref->curMBInfo.type == 0) //���ӹؼ���
							{
								if (globe->key_buf.key_index < 9) //������һ�����û���
								{
									globe->key_buf.key_index ++; //�½�һ������
									globe->new_key = false; //ȡ���½��ؼ��ֱ��
								}
								else //�޷�����Ӽ�ֵ�ˣ����ؼ�
								{
									return false;
								}
							}
							else /*if( pref->autoSend)*/	//�����볤���Զ����ֲ����¼���
							{
								SelectResult(buf, operation, slot1, globe, &pref->curMBInfo, SelectBySelector);
								if (! globe->in_create_word_mode)
								{
									(*operation) = pimeReActive;
								}
							}
						}
						//��ֵ����
						if (pref->curMBInfo.translate_offset != 0) //���ڼ�ֵת��
						{
							//ȡ��ֵ��Ӧ������
							tmp = KeyTranslate((Char)new_key, pref->curMBInfo.key_translate, GetTranslatedKey);
							if (tmp != NULL) //�ҵ�ƥ�������
							{
								new_key = (WChar)(*tmp);
							}
						}
						//��Ӽ�ֵ������
						if (pref->curMBInfo.syncopate_offset != 0) //�����Զ�����
						{
							//����׼�������Ĺؼ�������
							StrCopy(globe->cache, globe->key_buf.key[globe->key_buf.key_index].content);
							globe->cache[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							key_cache_length = globe->key_buf.key[globe->key_buf.key_index].length + 1; //���г���
							key_syncopate = pref->curMBInfo.key_syncopate;
							read_size = 0;
							//ѭ��ƥ���Զ���������
							while (read_size < pref->curMBInfo.syncopate_size)
							{
								sample_length = StrLen(key_syncopate); //��ǰ�������ڳ���
								//���ú������ƥ�䷨��ȡƫ����
								if (key_cache_length >= sample_length)
								{
									tmp = globe->cache + (key_cache_length - sample_length);
								}
								else
								{
									tmp = globe->cache;
								}
								//���������ڳ��Ƚ��бȽ�
								if (StrNCompare(tmp, key_syncopate, StrLen(tmp)/*StrLen(key_syncopate)*/) == 0) //ƥ�䣬�������ڻ��ִ���
								{
									if (key_cache_length <= sample_length) //����Ҫ����
									{
										StrCopy(globe->key_buf.key[globe->key_buf.key_index].content, tmp);
										globe->key_buf.key[globe->key_buf.key_index].length ++;
									}
									else if (globe->key_buf.key_index < 9) //��Ҫ���֣����д��ڿɷ�����»���
									{
										//����������
										StrCopy(globe->key_buf.key[globe->key_buf.key_index + 1].content, tmp);
										globe->key_buf.key[globe->key_buf.key_index + 1].length = StrLen(tmp);
										//�ж�
										(*tmp) = 0x00;
										//����ԭ����
										StrCopy(globe->key_buf.key[globe->key_buf.key_index].content, globe->cache);
										globe->key_buf.key[globe->key_buf.key_index].length = StrLen(globe->cache);
										globe->key_buf.key_index ++;
									}
									else //��Ȼ��Ҫ���֣���û�пɷ�����»���
									{
										StrCopy(globe->key_buf.key[globe->key_buf.key_index].content, tmp);
										globe->key_buf.key[globe->key_buf.key_index].length ++;
									}
									matched = true; //��ƥ��ı�־
									break;
								}
								key_syncopate += sample_length + 1;
								read_size += sample_length + 1;
							}
							if ((! matched)) //��δƥ�䣬�Ҳ���ƥ������
							{
								if (globe->key_buf.key_index < 9)
								{
									if (globe->key_buf.key[globe->key_buf.key_index].length > 0) //���ǵ�һ����Ԫ�����õ��»�����
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
							//�������
							MemSet(globe->cache, 128, 0x00);
						}
						else //�����ڣ�ֱ�����
						{
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							globe->key_buf.key[globe->key_buf.key_index].length ++;
						}
					}
					else //��Ӣ��״̬
					{
						if (globe->key_buf.key_index <= 9 && globe->key_buf.key[globe->key_buf.key_index].length < 99)
						{ //�����㹻���������
							globe->key_buf.key[globe->key_buf.key_index].content[globe->key_buf.key[globe->key_buf.key_index].length] = (Char)new_key;
							globe->key_buf.key[globe->key_buf.key_index].length ++;
							//Ӣ��ģʽ
							globe->english_mode = true;
							//�������
							InitMBRecord(globe);
							InitResult(globe);
							//��ҳ���
							globe->no_prev = true;
							globe->no_next = true;
						}
					}
				}
				else //�Ƿ��ַ�
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
//��ȡ���ϵ�����ָ��
static FileRef DmOpenDatabaseOnCard(stru_MBInfo *mb_info, stru_Globe *globe)
{
	UInt16				vol_ref;
	UInt32				vol_iterator = vfsIteratorStart;
	FileRef				file_ref = 0;
	
	//ȡ��ָ��
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref > 0) //������
	{
		//��������·��
		StrCopy(globe->cache, PIME_CARD_PATH);
		StrCat(globe->cache, mb_info->file_name);
		VFSFileOpen(vol_ref, globe->cache, vfsModeRead, &file_ref);
		MemSet(globe->cache, 50, 0x00);
	}
	
	return file_ref;
}
//--------------------------------------------------------------------------
//���������
static WinHandle CreateIMEForm(FormType **ime_form, RectangleType *ime_form_rectangle, stru_Globe *globeP)
{
	PointType		inspt_position;
	WinHandle		offset_buf = NULL;
	UInt16			err;
	Coord			extenty;
	
	InsPtGetLocation(&inspt_position.x, &inspt_position.y); //ȡ�������
	
	globeP->resultRect[0].topLeft.x = 63; globeP->resultRect[0].topLeft.y = 16; globeP->resultRect[0].extent.x = 30;
	globeP->resultRect[1].topLeft.x = 33; globeP->resultRect[1].topLeft.y = 16; globeP->resultRect[1].extent.x = 29;
	globeP->resultRect[2].topLeft.x = 94; globeP->resultRect[2].topLeft.y = 16; globeP->resultRect[2].extent.x = 29;
	globeP->resultRect[3].topLeft.x = 3; globeP->resultRect[3].topLeft.y = 16; globeP->resultRect[3].extent.x = 29;
	globeP->resultRect[4].topLeft.x = 124; globeP->resultRect[4].topLeft.y = 16; globeP->resultRect[4].extent.x = 29;
	
	if (globeP->settingP->displayFont == largeFont || globeP->settingP->displayFont == largeBoldFont)
	{
		if (globeP->settingP->curMBInfo.type == 1 && globeP->settingP->curMBInfo.gradually_search)	//������ʾ
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
		if (globeP->settingP->curMBInfo.type == 1 && globeP->settingP->curMBInfo.gradually_search)	//������ʾ
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
					if ((*operationP) == 0xFF) //ѡ����ɺ�δָʾ�˳������
					{
						if (globeP->in_create_word_mode) //���������ģʽ�������δ���
						{
							//�������
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
			//���������²������Ĳ���
			if (KeywordHandler(chr, operationP, globeP, globeP->settingP, globeP->bufP))
			{
				if ((*operationP) == 0xFF)
				{
					if (! globeP->english_mode)
					{
						//�������
						SearchMB(globeP, &globeP->settingP->curMBInfo);
					}
					
					shouldRedrawForm = true;
					
					/*//�Ƿ�ﵽ�Զ����ֵ�����
					if ((! globeP->in_create_word_mode) && globeP->settingP->curMBInfo.type == 1 && globeP->key_buf.key[0].length == globeP->settingP->curMBInfo.key_length && HasOnlyOneResult(globeP))
					{ //�ﵽ�����Զ����ֵ��������Զ�����
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
					if ((! globeP->in_create_word_mode) && globeP->db_file_ref == NULL)//�Զ��尴�����������˵�
						{ //��option��ϻ�menu������ѡ�ּ����Ҳ����������ģʽ
							globeP->imeMenuP = MenuGetActiveMenu(); //��ȡ��ǰ�˵�
							if (globeP->imeMenuP == NULL) //�˵�δ���أ�������
							{
								globeP->imeMenuP = MenuInit(menuSpecial);
								MenuSetActiveMenu(globeP->imeMenuP);
								if (globeP->settingP->curMBInfo.type == 0) //��������������ֶ����ѡ��
								{
									MenuHideItem(miCreateWord);
								}
								/*if (StrNCaselessCompare(globeP->settingP->curMBInfo.file_name + (StrLen(globeP->settingP->curMBInfo.file_name)-3), "GBK", 3) != 0) //GB��������ַ��ļ���ѡ��
								{
									MenuHideItem(miGB);
									MenuHideItem(miGBK);
								}*/
							}
							MenuDrawMenu(globeP->imeMenuP); //��ʾ�˵�
							globeP->in_menu = true; //�˵��򿪱�ʶ
							isKeyHandled = true;
						}
				}
				else
				{
					switch (chr)
					{
						case vchrMenu: //���������˵�
						{
							if ((! globeP->in_create_word_mode) && globeP->db_file_ref == NULL)
							{ //��option��ϻ�menu������ѡ�ּ����Ҳ����������ģʽ
								globeP->imeMenuP = MenuGetActiveMenu(); //��ȡ��ǰ�˵�
								if (globeP->imeMenuP == NULL) //�˵�δ���أ�������
								{
									globeP->imeMenuP = MenuInit(menuSpecial);
									MenuSetActiveMenu(globeP->imeMenuP);
									if (globeP->settingP->curMBInfo.type == 0) //��������������ֶ����ѡ��
									{
										MenuHideItem(miCreateWord);
									}
									/*if (StrNCaselessCompare(globeP->settingP->curMBInfo.file_name + (StrLen(globeP->settingP->curMBInfo.file_name)-3), "GBK", 3) != 0) //GB��������ַ��ļ���ѡ��
									{
										MenuHideItem(miGB);
										MenuHideItem(miGBK);
									}*/
								}
								MenuDrawMenu(globeP->imeMenuP); //��ʾ�˵�
								globeP->in_menu = true; //�˵��򿪱�ʶ
								isKeyHandled = true;
							}
							break;
						}
						case chrUpArrow: //�Ϸ�ҳ
						case vchrPageUp:
						case vchrRockerUp:
						case keyComma: //101���Ϸ�ҳ
						{
							if (! globeP->no_prev && (chr==keyComma ? globeP->settingP->KBMode == KBModeExtFull: true)) //�����Ϸ�
							{
								//��������һҳ�ĵ�һ�����
								RollBackResult(globeP); //��ǰҳ
								RollBackResult(globeP); //��һҳ
								globeP->cursor = 0;
								
								shouldRedrawForm = true;
								isKeyHandled = true;
							}
							break;
						}
						case chrDownArrow: //�·�ҳ
						case vchrPageDown:
						case vchrRockerDown:
						case keyPeriod:  //101���·�ҳ
						{
							if (! globeP->no_next && (chr==keyPeriod ? globeP->settingP->KBMode == KBModeExtFull: true)) //�����·�
							{
								globeP->no_prev = false;
								globeP->cursor = 0;
								
								shouldRedrawForm = true;
								isKeyHandled = true;
							}
							break;
						}
						case keyReturn: //��������ֶ���ʣ��򷵻�Ӣ��
						{
							SelectResult(globeP->bufP, operationP, 0, globeP, &globeP->settingP->curMBInfo, SelectByEnterKey);
							
							isKeyHandled = true;
							break;
						}
						case chrLeftArrow: //ѡ�ֹ������
						case vchrRockerLeft:
						{
							if(!globeP->english_mode)//Ӣ��ģʽ���޺�ѡ��
								MoveResultCursor(globeP, cursorLeft);
							
							shouldRedrawForm = true;
							isKeyHandled = true;
							break;
						}
						case chrRightArrow: //ѡ�ֹ������
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
						case keyNine: //�Դʶ���
						{
							SelectResult(globeP->bufP, operationP, (slot1 << globeP->cursor), globeP, &globeP->settingP->curMBInfo, chr-keyZero);
							if ((*operationP) == 0xFF && globeP->in_create_word_mode) //���������ģʽ�������δ���
							{
								//�������
								SearchMB(globeP, &globeP->settingP->curMBInfo);
								
								shouldRedrawForm = true;
							}
							isKeyHandled = true;
							break;
						}
						case vchrRockerCenter:
						case vchrHardRockerCenter: //���ѡ��
						{
							if ((! (eventP->data.keyDown.modifiers & willSendUpKeyMask)) || globeP->settingP->isTreo != isTreo650)
							{
								SelectResult(globeP->bufP, operationP, (slot1 << globeP->cursor), globeP, &globeP->settingP->curMBInfo, SelectBySelector);
								if ((*operationP) == 0xFF && globeP->in_create_word_mode) //���������ģʽ�������δ���
								{
									//�������
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
		case frmOpenEvent:	//�������
		{
			if (eventP->data.frmOpen.formID == frmIMEForm)
			{
				//�����뷨����
				globeP->draw_buf = CreateIMEForm(&globeP->imeFormP, &globeP->imeFormRectangle, globeP);
				if(globeP->settingP->showGsi) FrmNewGsi(&globeP->imeFormP, globeP->settingP->choice_button?114:132, 3);
				else FrmNewGsi(&globeP->imeFormP, 160, 160);				
				FrmSetActiveForm(globeP->imeFormP);				
				FrmDrawForm(globeP->imeFormP);							
				GsiEnable(true);
				GrfInitState();
				
				globeP->imeFormP = FrmGetActiveForm();	//ˢ��һ�鴰��ָ��
				
				
				//�����ʼ����
				if (KeywordHandler(globeP->initKey, operationP, globeP, globeP->settingP, globeP->bufP))
				{
					if ((*operationP) == 0xFF)
					{
						if (! globeP->english_mode)
						{
							//�������
							SearchMB(globeP, &globeP->settingP->curMBInfo);
						}
						
						//�Ƿ�ﵽ�Զ����ֵ�����
						if ((! globeP->in_create_word_mode) && globeP->settingP->curMBInfo.type == 1 && globeP->key_buf.key[0].length == globeP->settingP->curMBInfo.key_length && HasOnlyOneResult(globeP))
						{ //�ﵽ�����Զ����ֵ��������Զ�����
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
				//�Ƿ�ﵽ�Զ����ֵ�����
				if (globeP->settingP->autoSend && (! globeP->in_create_word_mode) && globeP->settingP->curMBInfo.type == 1 && globeP->key_buf.key[0].length == globeP->settingP->curMBInfo.key_length && HasOnlyOneResult(globeP))
				{ //�ﵽ�����Զ����ֵ��������Զ�����
					SelectResult(globeP->bufP, operationP, slot1, globeP, &globeP->settingP->curMBInfo, SelectBySelector);
				}				
				isEventHandled = true;
			}
			break;
		}
		case winEnterEvent: //�Ƿ��˳��˵�
		{
			if (globeP->imeFormP)
			{
				if (globeP->in_menu && eventP->data.winEnter.enterWindow == (WinHandle)globeP->imeFormP)
				{ //�ǴӲ˵��˳�
					globeP->in_menu = false;
					isEventHandled = true;
				}		
			}
			break;
		}
		case menuEvent: //�˵��¼�
		{
			switch (eventP->data.menu.itemID)
			{
				case miCreateWord: //�򿪹������������ģʽ
				{
					//���ñ�ʶ
					globeP->in_create_word_mode = true;
					//�ػ������
					if (globeP->result_head.next != (void *)&globeP->result_tail)
					{
						RollBackResult(globeP);
					}
					break;
				}
				case miDeleteWord: //ɾ������
				{
					if (DeleteWord(globeP)) //���¼������
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
				case miMoveAhead: //ǿ��ǰ�ô���
				{
					if (MoveWordToTop(globeP, false)) //ǿ����ǰ
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
				case miSetStatic: //�̶��ִ�
				{
					if (MoveWordToTop(globeP, true)) //ǿ����ǰ���̶�
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
				case miUnsetStatic: //ȡ���̶�
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
		case penDownEvent: //�����ر�������ѡ��
		{
			if ((! globeP->in_menu))
			{
				if (! RctPtInRectangle(eventP->screenX, eventP->screenY, &globeP->imeFormRectangle))//�����˳�
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
					if(RctPtInRectangle(eventP->screenX, eventP->screenY, &R2))//�޸ı���
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
						if(eventP->screenX<77)//�Ƿ񽫹�궨λ�������
						{
							for(i=0;i<key_lengths;i++)
								EvtEnqueueKey(chrLeftArrow, 0, 0);
						}
						FrmCustomResponseAlert (alertInput, "Enter", "new", "code", globeP->cache, 512, NULL);
						if(globeP->cache[0])//����������
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
		case ctlSelectEvent: //��ť
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
					globeP->imeMenuP = MenuGetActiveMenu(); //��ȡ��ǰ�˵�
					if (globeP->imeMenuP == NULL) //�˵�δ���أ�������
					{
						globeP->imeMenuP = MenuInit(menuSpecial);
						MenuSetActiveMenu(globeP->imeMenuP);
						if (globeP->settingP->curMBInfo.type == 0) //��������������ֶ����ѡ��
						{
							MenuHideItem(miCreateWord);
						}
					}
					MenuDrawMenu(globeP->imeMenuP); //��ʾ�˵�
					globeP->in_menu = true; //�˵��򿪱�ʶ
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
* ������:    SLWinDrawBitmap
*
*   ����:    ������Դ����/ID��Ӧ��λͼ
*
* ����ֵ:    ��
*
*   ��ʷ:	��  ��		��  ��			��        ��
*			------		----------		-----------------
*			Sean		2003/08/14		��ʼ�汾
*			Bob			2008/07/03		�޸���x,y����
*******************************************************************/
void SLWinDrawBitmap
(
        DmOpenRef dbP,        // (in)��Դ�ļ����ݿ�ָ��
        UInt16 uwBitmapIndex, // (in)λͼ��Դ��Index����ID
        Coord x,              // (in)λͼ�������½ǵ�x����
        Coord y,              // (in)λͼ�������½ǵ�y����
        Boolean bByIndex      // (in)true��������Դ��������ȡBitmap
                              //     false��������ԴID����ȡBitmap
                              // ���Ϊtrue��������dbP����
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
       
        // ��ȡλͼ��Դ
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

        // �ж�Windows Manager�İ汾
        FtrGet(sysFtrCreator, sysFtrNumWinVersion, &udwWinVersion);
        if (udwWinVersion >= 4)
        {
                bHiRes = true;
        }
        else
        {
                bHiRes = false;
        }

        // ������Ǹ߷ֱ��ʣ�ֱ�ӻ���
        if (! bHiRes)
        {
                WinDrawBitmap (resourceP, x, y);
                MemPtrUnlock(resourceP);
                DmReleaseResource(resourceH);
                return;
        }


        // �ж�λͼ�Ƿ��Ǹ߷ֵģ�������ǣ�ֱ�ӻ���
        if (BmpGetDensity(resourceP) == kDensityLow)
        {
                WinDrawBitmap (resourceP, x, y);
                MemPtrUnlock(resourceP);
                DmReleaseResource(resourceH);
                return;
        }
       
        // ������ø߷ֱ��ʽ��л���
        // ����Native����ϵ
        uwPrevCoord = WinSetCoordinateSystem(kCoordinatesNative);

        // �������ⴰ���л�ͼ�����õͷֱ��ʻ���
        bitmapP = NULL;
        bmpP = NULL;
       
        winH = WinCreateOffscreenWindow(320, 320, nativeFormat, &err);
        if (err)
        {
                // �ָ�����ϵ
                WinSetCoordinateSystem(uwPrevCoord);       
                return;
        }
       
        bitmapP = WinGetBitmap(winH);
        BmpSetDensity(bitmapP, kDensityLow);
        oldWinH = WinSetDrawWindow(winH);
        bmpH = DmGetResourceIndex(dbP, uwBitmapIndex);
        ErrFatalDisplayIf(! bmpH, "Cannot open the bitmap.");
        bmpP = (BitmapPtr)MemHandleLock(bmpH);
       
        // ��ȡͼ���С
        BmpGetDimensions(bmpP, &wWidth, &wHeight, 0);
        WinDrawBitmap(bmpP, 0, 0);

        typRect.topLeft.x = 0;
        typRect.topLeft.y = 0;
        typRect.extent.x = wWidth;
        typRect.extent.y = wHeight;

        MemHandleUnlock(bmpH);
        DmReleaseResource(bmpH);

        // ���Ƶ�ԭ���Ĵ��ڣ��Ը߷ֱ��ʻ���
        BmpSetDensity(bitmapP, kDensityDouble);
        WinSetDrawWindow(oldWinH);
        WinCopyRectangle(winH, 0, &typRect, x, y, winPaint);
        WinDeleteWindow(winH,0);
       
        // �ָ�����ϵ
        WinSetCoordinateSystem(uwPrevCoord);       
}
//--------------------------------------------------------------------------
//������¼�����
static UInt8 PIMEEventHandler(WChar *chrP, Char *bufP, stru_Pref *settingP, FormType *curFormP)
{
	EventType		event;
	UInt8			operation = 0xFF;
	UInt16			error;
	stru_Globe		*globeP;
	Char 			bufK[5]=""; 
	
	//��ʼ������
	globeP = (stru_Globe *)MemPtrNew(sizeof(stru_Globe));
	MemSet(globeP, sizeof(stru_Globe), 0x00);
	globeP->settingP = settingP;
	globeP->bufP = bufP;
	globeP->initKey = (*chrP);
	globeP->result_head.next = (void *)&globeP->result_tail;
	globeP->result_tail.prev = (void *)&globeP->result_head;
	
	FntSetFont(settingP->displayFont);
	globeP->curCharWidth = FntCharsWidth("��", 2);
	globeP->curCharHeight = FntCharHeight();
	FntSetFont(stdFont);
	
	//�����ݿ�
	if (settingP->curMBInfo.inRAM || settingP->dync_load) //������ڴ�
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
	
	//�ر����ݿ�
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
//��������ֶ���ʶԻ���
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
	
	//��ʼ������
	globe = (stru_Globe *)MemPtrNew(stru_Globe_length);
	MemSet(globe, stru_Globe_length, 0x00);
	globe->result_head.next = (void *)&globe->result_tail;
	globe->result_tail.prev = (void *)&globe->result_head;
	//�����ݿ�
	globe->db_ref = DmOpenDatabaseByTypeCreator(pref->curMBInfo.db_type, appFileCreator, dmModeReadWrite);
	//�������
	MemSet(index, 5, 0x00);
	//���ֶ���ʶԻ���
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
						case btnSaveWord: //�����
						{
							key_length = FldGetTextLength(key_field); //��ȡ�ؼ��ֳ���
							word = FldGetTextPtr(word_field);
							word_length = StrLen(word); //��ȡ���鳤��
							key = FldGetTextPtr(key_field); //��ȡ�ؼ�������
							//�ؼ�����û�����ܼ����Ϸ�������ֵ���ȷ������Ҫ�������ֲ��ֲ��ǵ��֣��������
							if (KeyHasWildChar(key, key_length, &pref->curMBInfo) && key_length <= pref->curMBInfo.key_length && key_length > 0 && word_length > 2)
							{
								FrmCustomAlert(alertCreateWordErr, "Codes contains wild character.", "", "");
							}
							else
							{
								//�������������ݵ�Ԫ����������β��ʶ\0x01���ò�����SaveWord()����ӣ�
								content = (Char *)MemPtrNew(key_length + word_length + 1); //�����ڴ�
								StrCopy(content, key); //�ؼ���
								StrCat(content, word); //����
								//����ô��������
								for (i = 0; i < key_length; i ++)
								{
									index[j] = key[i];
									if (j < 3)
									{
										j ++;
									}
								}
								SaveWord(index, content, globe, &pref->curMBInfo);
								MemPtrFree(content); //�ͷ��ڴ�
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

	//�ر����ݿ�
	DmCloseDatabase(globe->db_ref);
	//�ͷ��ڴ�
	MemPtrFree(globe);
	//�رնԻ���
	FrmReturnToForm(0);
}
//--------------------------------------------------------------------------
//ת������ΪTreo�����ϵ�Ӣ�ı��
static void TreoKBEnglishPunc(Char *str, WChar curKey)
{
	Char punc_str[27]="&#84156$@!:',?\"p/2-3)9+7(*"; //Treo����
	Int16 idx=curKey-keyA;
	if(idx>=0 && idx<=25)
		str[0]=(idx>=0 && idx<=25)? punc_str[idx]:(Char)curKey;
}

//--------------------------------------------------------------------------
//Treo���̷��Ŵ���
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
				case keyPeriod: //���
				{
					if (pref->english_punc)
					{
						StrCopy(str, ".");
					}
					else
					{
						StrCopy(str, "��");
					}
					break;
				}
			}
		}
		else if (pref->hasShiftMask)
		{
			switch (curKey)
			{
				case keyPeriod: //�ֺ�
				{
					//StrCopy(str, "��");
					StrCopy(str, pref->CustomLPShiftPeriod);
					break;
				}
				case keyBackspace: //���ۺ�
				{
					//StrCopy(str, "����");
					StrCopy(str, pref->CustomLPShiftBackspace);
					break;
				}
			}
		}
		else if (pref->hasOptionMask)
		{
			switch (curKey)
			{
				case keyPeriod: //Ӣ�ľ��
				{
					StrCopy(str, ".");
					break;
				}
				case keyBackspace: //ʡ�Ժ�
				{
					//StrCopy(str, "����");
					StrCopy(str, pref->CustomLPOptBackspace);
					break;
				}
				default:
					TreoKBEnglishPunc(str, curKey);	
			}
		}
	}
	else //����
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
	if(pref->fullwidth &&(pref->num_fullwidth || str[0]>'9' || str[0]<'0')/* �Ƿ�Ϊȫ������*/)	
		TreoKBFullwidth(str);//ȫ�Ƿ���
	TreoKBDynamicPunc(str);//�������
	return str;
}
//--------------------------------------------------------------------------
//���ü��̷��Ŵ���
static Char *ExtKBPuncEventHandler(WChar curKey, UInt16 curKeyCode, stru_Pref *pref, Boolean isLongPress)
{
	Char	*str = NULL;
	
	str = MemPtrNew(5);
	MemSet(str, 5, 0x00);
	
	switch (curKey)
	{
		case 33: //̾��
		{
			StrCopy(str, "��");
			break;
		}
		case 34: //˫����
		{
			StrCopy(str, "����");
			break;
		}
		case 35: //���˺�
		{
			StrCopy(str, "��");
			break;
		}
		case 39: //������
		{
			StrCopy(str, "����");
			break;
		}
		case 40: //����
		{
			StrCopy(str, "����");
			break;
		}
		case 41: //������
		{
			StrCopy(str, "����");
			break;
		}
		case 42: //�ٺ�
		{
			StrCopy(str, "��");
			break;
		}
		case 43: //ʡ�Ժ�
		{
			StrCopy(str, "����");
			break;
		}
		case 44: //����
		{
			StrCopy(str, "��");
			break;
		}
		case 45: //���ۺ�
		{
			StrCopy(str, "����");
			break;
		}
		case 46: //���
		{
			StrCopy(str, "��");
			break;
		}
		case 58: //ð��
		{
			StrCopy(str, "��");
			break;
		}
		case 63: //�ʺ�
		{
			StrCopy(str, "��");
			break;
		}
	}
	return str;
}
//--------------------------------------------------------------------------
//���ݸ�����λ�ã���ȡ����ʹ�õ�������ʼλ��
static UInt16 GetStartPosition(UInt16 position, Char *text)
{
	UInt16		text_length;
	
	text_length = StrLen(text);
	if (position >= text_length) //������Χ������
	{
		position = text_length - 1;
	}
	//�ӵ�ǰλ����ǰ�ƶ���ֱ���ҵ��ָ����� �����򵽴��ı�ͷ��
	while (text[position] != ' ' && position > 0)
	{
		position --;
	}
	if (position > 0) //�����ҵ��ָ��������������
	{
		position ++; //ָ��ָ�������ַ�
	}
	return position;
}
//--------------------------------------------------------------------------
//���ݸ�������ʼλ�ã���ȡ��ѡ�����ݵĽ���λ��
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
//��ȡ�ַ���˳����
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
//����, ���麺����Ϣ�¼�
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

	if ((pref->activeStatus & inJavaMask))//�����JAVA��ȡ��
		return;

	FldGetSelection (pref->current_field , &start, &pos);
	if (pos>start)//�����ֱ�ѡ�У��������
	{
		FldSetInsertionPoint(pref->current_field, pos);
		FldSetInsPtPosition(pref->current_field, pos);
	}
	else
		pos=FldGetInsPtPosition(pref->current_field);
	outLen=TxtGetPreviousChar(FldGetTextPtr (pref->current_field), pos, &curChar);
	*txtlen = 0;
	
	if (outLen!=2 || !(pref->altChar || pref->suggestChar))//���Ǻ��ֻ�����
		return;
		
	recordIndex = GetCharIndex(curChar);//��ȡ�ַ�˳���

	while (vol_iterator != vfsIteratorStop)//��ȡ���濨����,ȡ��ָ��
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}

	//�򿪺�����Ϣ���ݿ�
	dbRef = DmOpenDatabaseByTypeCreator('dict', 'pIME', dmModeReadOnly);
	if (dbRef == NULL && vol_ref > 0)//���ڴ���û�ҵ����ݿ⣬�����ڿ�����
	{
		VFSFileOpen(vol_ref, PIME_CARD_PATH_DICT, vfsModeRead, &db_file_ref);		
	}
	DmGetRecordFromCardAndRAM(dbRef, db_file_ref, recordIndex, &memHandle);	
	if(memHandle == NULL)
		exit = true;
	else//��ȡ�б�
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
	//�ر����ݿ�
	DmCloseDatabaseFromCardAndRAM(dbRef, db_file_ref);
	//�˳�
	if(exit)
		return;
	 
	//�½�����
	numItems =  numberOfStrings < SUGGEST_LIST_HEIGHT ? numberOfStrings : SUGGEST_LIST_HEIGHT;
	height = numItems * 11 + 4;
	
	InsPtGetLocation(&inspt_position.x, &inspt_position.y); //ȡ�������
	inspt_position.x+=1;
	inspt_position.y-=2;
	(*tray_form) = FrmNewForm(frmAlt, NULL, inspt_position.x > 106 ? 106 : inspt_position.x , inspt_position.y + height > 160 ? 160 - height: inspt_position.y, 54, height, false, NULL, NULL, NULL);	
	LstNewList ((void **)tray_form, lstAlt, 2, 2, 50, height, stdFont, numItems , NULL);
	lstP = (ListType *)FrmGetObjectPtr(*tray_form, FrmGetObjectIndex(*tray_form, lstAlt));
	LstSetDrawFunction(lstP,(ListDrawDataFuncPtr)myDrawFunc);	//�Զ��廭��	
	LstSetListChoices ( lstP, itemsPtr+(pref->altChar? 0:numberOfAltStrings), numberOfStrings );
	//FrmSetFocus(*tray_form, FrmGetObjectIndex(*tray_form, lstAlt));   
    FrmSetActiveForm(*tray_form);
    FrmDrawForm(*tray_form);
    		
	//�¼�ѭ��
	do
	{
		//��ȡ�¼�
		EvtGetEvent(&event, evtWaitForever);
		//�¼�����
		if (! SysHandleEvent(&event))
		{
			if (event.eType == keyDownEvent) //�����¼�
			{				
				KeyTransfer2(&key, &event, pref);//��ֵת��
				switch (key)
				{
					case 0:
					case vchrHardRockerCenter:
					case vchrRockerCenter://ѡ��
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
							if(tmp[(*txtlen)-1]==' ')//�ַ�ת��
							{
								(*txtlen)--;//ȥ���ո�															
								FldSetSelection(pref->current_field, pos-outLen, pos);//ɾ����ǰ��
							}
						}
						if((*txtlen)>1 && tmp[(*txtlen)-1]=='-')//ȥ���ָ���
								(*txtlen)--;//ȥ���ָ���
						StrNCopy(buf, tmp, (*txtlen));
						exit = true;
						break;				
					}
					case vchrPageDown:
					case vchrRockerDown:
					case chrDownArrow://����
					case hsKeySymbol: //ѭ��
						LstSetSelection(lstP, (LstGetSelection(lstP) + 1< numberOfStrings ) ? (LstGetSelection(lstP) + 1) : 0);
						break;
					case vchrPageUp:
					case vchrRockerUp:
					case chrUpArrow://����
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
				key=0;//�����ֵ
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

	
	//�ͷ��ڴ�    
    MemHandleUnlock(memStringList);
	MemHandleFree(memStringList);
	//�رմ���
	FrmEraseForm(*tray_form);
	FrmReturnToForm(0);
}
//
//������
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
				
	//�򿪴���
	tray_form = FrmInitForm(frmPunc);
	//�����ڴ�
	punc_field = (FieldType **)MemPtrNew(28);	
	//��ȡ�ı���ָ��
	punc_field[0] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP1));
	punc_field[1] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP2));
	punc_field[2] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP3));
	punc_field[3] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP4));
	punc_field[4] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP5));
	punc_field[5] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP6));
	punc_field[6] = (FieldType *)FrmGetObjectPtr(tray_form, FrmGetObjectIndex(tray_form, fldP7));
	//�����ı������ݣ��������ַ�����
	//�ѱ����Ŵ��󶨵��ı���
	for (i = 0; i < 7; i ++)
	{
		FldSetTextPtr(punc_field[i], punc[i]);
		//FldDrawField(punc_field[i]);
	}
	//�趨��ѡ����ı�
	FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
	//FldDrawField(punc_field[row]);
	FrmSetActiveForm(tray_form);
	FrmDrawForm(tray_form);
	
	//�¼�ѭ��
	do
	{
		//��ȡ�¼�
		EvtGetEvent(&event, evtWaitForever);
		//�¼�����
		if (! SysHandleEvent(&event))
		{
			if (event.eType == keyDownEvent) //�����¼�
			{
				//��ֵת��
				KeyTransfer2(&key, &event, pref);
				switch (key)
				{
					case 28:
					case vchrRockerLeft: //����
					{
						//�����ǰ��ѡ��
						FldSetSelection(punc_field[row], col, col);
						//�޸�����
						if (col > 1)
						{
							col -= 2;
						}
						else
						{
							col = StrLen(punc[row]) - 1;
						}
						col = GetStartPosition(col, punc[row]);
						//�趨ѡ������
						FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
						FldDrawField(punc_field[row]);
						break;
					}
					case 29:
					case vchrRockerRight: //����
					{
						//�����ǰ��ѡ��
						FldSetSelection(punc_field[row], col, col);
						//�޸�����
						col += (GetEndPosition(col, punc[row]) - col);
						if (col == StrLen(punc[row]))
						{
							col = 0;
						}
						col = GetStartPosition(col, punc[row]);
						//�趨ѡ������
						FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
						FldDrawField(punc_field[row]);
						break;
					}
					case 30:
					case vchrPageUp: //����
					case 31:
					case vchrPageDown: //����
					{
						//�����ǰ��ѡ��
						FldSetSelection(punc_field[row], col, col);
						//�޸�����
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
						//�趨ѡ������
						FldSetSelection(punc_field[row], col, GetEndPosition(col, punc[row]));
						FldDrawField(punc_field[row]);
						break;
					}
					case 0x20:
					case vchrRockerCenter: //ѡ��
					{
						//��ȡѡ�������
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
				//�����ֵ
				key = 0;
			}
			else
			{
				FrmHandleEvent(tray_form, &event);
			}
		}
	}while(event.eType != appStopEvent && exit == false);
	//������Ŵ����ı���İ󶨣����ͷ��ڴ�
	for (i = 0; i < 7; i ++)
	{
		FldSetTextPtr(punc_field[i], NULL);
		//MemPtrFree(punc[i]);
	}
	//�ͷ��ڴ�    
	MemHandleUnlock(listHandle);
	MemHandleFree(listHandle);	
	//MemPtrFree(punc);
	MemPtrFree(punc_field);
	//�رմ���
	FrmReturnToForm(0);
}*/

//
//���������, ��ǿ
static void PuncTrayEventHandler(Char *buf, UInt16 *txtlen, stru_Pref *pref)
{
	UInt16			i;
	Boolean			exit = false;
	WChar			key = 0;	
	FormType		*tray_form;		
	ListType 		*listP;
	EventType		event, ep;

	//�򿪴���
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
			
	//�¼�ѭ��
	do
	{
		//��ȡ�¼�
		EvtGetEvent(&event, evtWaitForever);
		//�¼�����
		if (! SysHandleEvent(&event))
		{
			if (event.eType == keyDownEvent) //�����¼�
			{
				//��ֵת��
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
				//�����ֵ
				key = 0;
			}
			else if (event.eType == ctlSelectEvent && event.data.ctlSelect.controlID!=triggerPunc) //�����¼�
			{
				StrCopy(buf, CtlGetLabel (event.data.ctlSelect.pControl));
				TreoKBDynamicPunc(buf); //��̬�������
				*txtlen=StrLen(buf);
				exit=true;
			}
			else if (event.eType == popSelectEvent)//��ʾ����
			{
				FrmHandleEvent(tray_form, &event);
				pref->PuncType = event.data.popSelect.selection;
				if(pref->PuncType)//��ȡ��Դ�еķ����б�
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
				else//�Զ������
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
	//�رմ���
	FrmReturnToForm(0);
}


//--------------------------------------------------------------------------
//����л�����
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

	//��ȡ���濨����
	//ȡ��ָ��
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	//�������Ϣ���ݿ�
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	//�����ڴ�
	full_path = (Char *)MemPtrNew(100);
	mb_list = (Char **)MemPtrNew((mb_num << 2));
	mb_index = (UInt16 **)MemPtrNew((mb_num << 2));
	//ȡ�����õ������
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
			if (mb_list_unit->MBDbType == pref->curMBInfo.db_type) //�趨�б�ѡ����
			{
				list_selection = j;
			}
			//�����ڴ�
			mb_list[j] = (Char *)MemPtrNew(32);
			mb_index[j] = (UInt16 *)MemPtrNew(2);
			if (mb_list_unit->inRAM) //�ڴ������ȡ����
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
					//����·��
					StrCopy(full_path, PIME_CARD_PATH);
					StrCat(full_path, mb_list_unit->file_name);
					//��ȡ���ݿ��ļ�����
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
			//��¼����¼�ļ�¼��
			(*mb_index[j]) = i;
			j ++;
		}
		MemHandleUnlock(record_handle);
	}
	//�ر����ݿ�
	DmCloseDatabase(dbRef);

	
	if(tempSwitch) //��ʱ�л�
	{
		if(pref->activeStatus & tempMBSwitchMask) //��ʱ״̬
		{
			pref->activeStatus &= (~tempMBSwitchMask); //������״̬
			list_selection--;
			if(list_selection==-1)
				list_selection=mb_num-1;
			ShowStatus(frmMainSwitchMB, mb_list[list_selection], 100);			
		}
		else //����״̬
		{
			pref->activeStatus |= tempMBSwitchMask; //����ʱ״̬
			list_selection++;
			if(list_selection==mb_num)//�����һ���ˣ��л�����һ��
				list_selection = 0;
			ShowStatus(frmTempSwitchMB, mb_list[list_selection], 300);		
		}		
	}	
	else if(pref->AutoMBSwich)//�Զ��л����
	{
		list_selection++;
		if(list_selection==mb_num)//�����һ���ˣ��л�����һ��
				list_selection = 0;		
		pref->activeStatus &= (~tempMBSwitchMask); //�ظ�������״̬
		ShowStatus(frmAutoSwitchMB, mb_list[list_selection], 400);
	}	
	else
	{
		//������л�����
		frmP = FrmInitForm(frmSwitchMB);
		lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstMB));
		//�����б�
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
		//��ȡ�����Ϣ
		GetMBInfoFormMBList(&pref->curMBInfo, (*mb_index[list_selection]), false, pref->dync_load);
		GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load);
		PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	}		
	//�ͷ��ڴ�
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

//---------------------��������----------------------------------------------------------------
//���Grf״̬
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
//���Grfָʾ���Ƿ�������״̬
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
//�ѽ��ѹ����̶��У�����Java��DTG�У�
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
//��ɫת��
UInt16 Make16BitRGBValue (UInt16 r, UInt16 g, UInt16 b)
{
    return (r & 0x1f << 11 | g & 0x3f << 5 | b & 0x1f);
}
//--------------------------------------------------------------------------
//JavaDTGģʽ��ɫ��
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
	
	// �ж�Windows Manager�İ汾
	FtrGet(sysFtrCreator, sysFtrNumWinVersion, &udwWinVersion);
	if (udwWinVersion >= 4)
	{
			bHiRes = true;
	}
	else
	{
			bHiRes = false;
	}
	
   // ������Ǹ߷ֱ��ʣ�ֱ�ӻ���
	if (! bHiRes)
	{
		//����
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
		//�н�
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
	
	
	// ������ø߷ֱ��ʽ��л���
	// ����Native����ϵ
	uwPrevCoord = WinSetCoordinateSystem(kCoordinatesNative);

	winH = WinCreateOffscreenWindow(320, 320, nativeFormat, &err);
	if (err)
	{
			// �ָ�����ϵ
			WinSetCoordinateSystem(uwPrevCoord);       
			return;
	}
	// �������ⴰ���л�ͼ�����õͷֱ��ʻ���
	
	bitmapP = WinGetBitmap(winH);
	BmpSetTransparentValue (bitmapP,transparentValue);
	BmpSetDensity(bitmapP, kDensityLow);
	oldWinH = WinSetDrawWindow(winH);

	//����
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
	//�н�
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


	// ���Ƶ�ԭ���Ĵ��ڣ��Ը߷ֱ��ʻ���
	BmpSetDensity(bitmapP, kDensityDouble);
	WinSetDrawWindow(oldWinH);
	WinCopyRectangle(winH, 0, &typRect, 160, 160, winPaint);
	WinDeleteWindow(winH,0);
   
	// �ָ�����ϵ
	WinSetCoordinateSystem(uwPrevCoord);  
}
/*
static void DrawPixel(RGBColorType *color,RGBColorType *colorEdge,UInt8 *javaStatusStyleX,UInt8 *javaStatusStyleY,UInt8 javaStyle)
{
	Int x,y;
	RGBColorType	prevRgbP;
	UInt8 width = *javaStatusStyleX;
	UInt8 height = *javaStatusStyleY;
	
	//����
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
	//�н�
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
//��׼ģʽ���
static void Output(WChar curKey, stru_Pref *pref)
{
	UInt8 			operation = 0xFF;
	UInt16			txtlen;
	Char			*buf, *realhead;
	//���仺��
	buf = (Char *)MemPtrNew(100);
	do
	{
		MemSet(buf, 100, 0x00);
		SetKeyRates(true, pref); //�ָ�Ĭ�ϰ����ظ���
		//������򣬽������룬�����ؽ���Ĳ�����
		operation = PIMEEventHandler(&curKey, buf, pref, FrmGetActiveForm());
		if (operation == pimeCreateWord) //���ֶ���ʶԻ���
		{
			//CreateWordEventHandler(buf, bufK, pref);
			SetKeyRates(false, pref); //�ӿ찴���ظ���
		}
		else
		{
			ClearGrfState(pref);//���ؽ��					
			SetKeyRates(false, pref); //�ӿ찴���ظ���
			txtlen = StrLen(buf);
			if (buf[0] == chrSpace)	//Ӣ�����������ͷ�ո񣬲��䵽���			
			{
				realhead = buf + 1;
				buf[txtlen] = chrSpace;
			}
			else
				realhead = buf;
			if ((pref->activeStatus & inJavaMask)) //Java��DTGģʽ
				EnqueueResultToKey(realhead, txtlen);
			else if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen)//��׼ģʽ
				FldInsert(pref->current_field, realhead, txtlen);
		}
	}while (operation == pimeReActive);
	//�ͷ��ڴ�
	MemPtrFree(buf);
	if(pref->activeStatus & tempMBSwitchMask)//��ʱ����ص�����״̬
		MBSwitchEventHandler(pref, true);
}
//------------------------
//����¼�����
static void ClearEvent(SysNotifyParamType *notifyPtr, EventType *ep)
{
	notifyPtr->handled = true;
	MemSet(ep, sizeof(EventType), 0x00);
	ep->eType = nilEvent;
}
//--------------------------------------------------------------------------
//Treo�����¼�����
static void TreoKeyboardEventHandler(SysNotifyParamType *notifyPtr, EventType *ep, WChar curKey, UInt16 curKeyCode, UInt16 curModifiers, stru_Pref *pref)
{
	Char			*buf;
	UInt8			operation = 0xFF;
	UInt16			txtlen=0;
	UInt16			cardNo;
	LocalID			dbID;
	RGBColorType	prevRgbP;
	/*Char			*doubleStr1,*doubleStr2,*doubleStr3,*doubleStr4,*doubleStr5,*doubleStr6,*doubleStr7,*doubleStr8,*doubleStr9,*doubleStr10,*doubleStr11,*doubleStr12,*doubleStr13,*doubleStr14,*doubleStr15; 
	
	doubleStr1="����";
	doubleStr2="����";
	doubleStr3="����";
	doubleStr4="����";
	doubleStr5="����";
	doubleStr6="����";
	doubleStr7="�ۣ�";
	doubleStr8="����";
	doubleStr9="����";
	doubleStr10="����";
	doubleStr11="����";
	doubleStr12="()";
	doubleStr13="{}";
	doubleStr14="[]";
	doubleStr15="<>";*/
	
	if ((curModifiers & willSendUpKeyMask)) //���»򳤰�
	{
		pref->keyDownDetected = true;
		if (! (pref->activeStatus & tempDisabledMask)) //��������״̬
		{
			if ((curModifiers & autoRepeatKeyMask) && (! pref->longPressHandled)) //�������Ұ���δ����
			{
				if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]) || curKey == keyPeriod || (pref->isTreo == isTreo600 && curKeyCode == hsKeySymbol))
				{ //�����������Ż�����
					//ȡ��Ӧ�����ı��
					buf = TreoKBPuncEventHandler(curKey, curKeyCode, pref, true);
					//���ؽ��
					txtlen = StrLen(buf);
					if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //δ�ﵽ�ı���ߴ�����
					{
						//����¼�����
						ClearEvent(notifyPtr, ep);
						//��Ǳ��ΰ����Ѵ���
						pref->isLongPress = true;
						pref->longPressHandled = true;
						if ((pref->activeStatus & inJavaMask)) //Java��DTGģʽ
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else //��׼ģʽ
						{
							FldInsert(pref->current_field, buf, txtlen);
							//if (StrLen(buf) == 4) //˫������㣬�ѹ���ƶ��������м�
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
					//�ͷ��ڴ�
					MemPtrFree(buf);
				}
			}
			else if ((pref->hasShiftMask || pref->hasOptionMask))
			{ //��һ��Shift��Option��ϼ���Ϣ
				if ((curKey == pref->MBSwitchKey || (curKey == pref->TempMBSwitchKey && !(pref->activeStatus & tempMBSwitchMask))) && pref->LongPressMBSwich && pref->hasOptionMask)
				{ //Opt+������л�
					//���ν��ռ�����Ϣ
					//SysCurAppDatabase(&cardNo, &dbID);
					//SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					ClearEvent(notifyPtr, ep);
					//������л�����
					MBSwitchEventHandler(pref, curKey != pref->MBSwitchKey);
					//����¼�����					
					//��Ǳ��ΰ����Ѵ���
					//pref->isLongPress = true;
					//pref->longPressHandled = true;
					//���Shift��Option����״̬
					ClearGrfState(pref);
					//�򿪽��ռ�����Ϣ
					//SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}
				else if (curKey == keyPeriod || curKey == keyBackspace || pref->opt_fullwidth) //Ӣ�ġ�.�������ġ��������򡰡�����
				{
					//ȡ������
					buf = TreoKBPuncEventHandler(curKey, curKeyCode, pref, false);
					txtlen = StrLen(buf);
					if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //δ�ﵽ�ı���ߴ�����
					{
						//����¼�����
						ClearEvent(notifyPtr, ep);
						//��Ǳ��ΰ����Ѵ���
						pref->isLongPress = true;
						pref->longPressHandled = true;
						//���Shift��Option����״̬
						ClearGrfState(pref);
						
						if ((pref->activeStatus & inJavaMask)) //Java��DTGģʽ
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else //��׼ģʽ
						{
							FldInsert(pref->current_field, buf, txtlen);
						}
						//�ͷ��ڴ�
						MemPtrFree(buf);
					}
				}
				else if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]))
				{ //�����ڳ�������ϼ������
					//��Ǳ��ΰ����¼��Ѵ���
					pref->longPressHandled = true;
				}
			}					
			else if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]) ||
					curKey == keyPeriod || curKeyCode == pref->IMESwitchKey || curKeyCode == pref->ListKey || curKeyCode == pref->PuncKey || (!pref->LongPressMBSwich  && (curKeyCode == pref->MBSwitchKey || curKeyCode == pref->TempMBSwitchKey)))
			{ //�����¼��Ѿ������������
				//����¼�����
				ClearEvent(notifyPtr, ep);
			}

		}
		else if ((curModifiers & autoRepeatKeyMask) && (! pref->longPressHandled)) //Ӣ��״̬�³�������
		{
			if ((curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1]) || curKey == keyPeriod || (pref->isTreo == isTreo600 && curKeyCode == hsKeySymbol))
			{ //�����������Ż�����
				//ȡ��Ӧ��Ӣ�ı��
				buf = (Char *)MemPtrNew(15);
				MemSet(buf, 15, 0x00);
				//TreoKBEnglishPunc(buf, curKey);
				//���ؽ��
				txtlen = StrLen(buf);
				if (txtlen > 0 && (FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //δ�ﵽ�ı���ߴ�����
				{
					//����¼�����
					ClearEvent(notifyPtr, ep);
					//��Ǳ��ΰ����Ѵ���
					pref->isLongPress = true;
					pref->longPressHandled = true;
					if ((pref->activeStatus & inJavaMask)) //Java��DTGģʽ
					{
						EnqueueResultToKey(buf, txtlen);
					}
					else //��׼ģʽ
					{
						FldInsert(pref->current_field, buf, txtlen);
					}
				}
				//�ͷ��ڴ�
				MemPtrFree(buf);
			}
		}
		else if (curKeyCode == pref->IMESwitchKey)
		{ //���뷨״̬�л�����Ӣ��״̬�±����£���������ȴ��ü���̧��
			//����¼�����
			ClearEvent(notifyPtr, ep);
		}
	}
	else if ((curModifiers & autoRepeatKeyMask)) //�������ɿ�
	{
		if (! (pref->activeStatus & tempDisabledMask)) //��������״̬
		{
			if ((! (pref->longPressHandled || pref->isLongPress)) && pref->keyDownDetected) //����δ������
			{
				pref->keyDownDetected = false;
				if (curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1] && StrChr(pref->curMBInfo.used_char, curKey) != NULL)
				{ //�����ǿ��Դ���ģ��������
					
					//����¼�����
					ClearEvent(notifyPtr, ep);
					//���ν��ռ�����Ϣ
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					Output(curKey, pref);//��׼ģʽ					
					//�򿪽��ܼ�����Ϣ
					SysNotifyRegister(cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}
				else if (curKey == keyPeriod)
				{ //����
					buf = TreoKBPuncEventHandler(curKey, curKeyCode, pref, false);
					txtlen = StrLen(buf);
					if (txtlen > 0)
					{
						//����¼�����
						ClearEvent(notifyPtr, ep);
						if ((pref->activeStatus & inJavaMask)) //Java��DTGģʽ
						{
							EnqueueResultToKey(buf, txtlen);
						}
						else if ((FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen) //��׼ģʽ
						{
							FldInsert(pref->current_field, buf, txtlen);
						}
						ClearGrfState(pref);
					}
					MemPtrFree(buf);
				}
				else if (curKeyCode == pref->IMESwitchKey)//��Ӣ���л������л���Ӣ��״̬����ʱ�ر����뷨
				{
					//����¼�����
					ClearEvent(notifyPtr, ep);
					//���״̬��־
					pref->activeStatus |= tempDisabledMask;
					//��¼���뷨״̬
					pref->last_mode = imeModeEnglish;
					if (pref->init_mode == initRememberFav)
					{
						SetInitModeOfField(pref);
					}
					//�ָ������ɫ�������ظ���
					SetKeyRates(true, pref);
					SetCaretColor(true, pref);
					if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask)) //Java��DTGģʽ������״̬����
					{
						if(pref->javaStatusStyle == Style1)
						{
							SLWinDrawBitmap(NULL, bmpEnIcon, 19,19, false);
							//WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
							//WinDrawChars("Ӣ", 2, 150, 148);
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
				{ //������Ϣ
					//���ν��ռ�����Ϣ
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					//����¼�����
					ClearEvent(notifyPtr, ep);
					buf = (Char *)MemPtrNew(15);
					MemSet(buf, 15, 0x00);
					curKey == pref->PuncKey ? PuncTrayEventHandler(buf, &txtlen, pref):AltEventHandler(buf, &txtlen, pref);
					if(txtlen)//���ؽ��
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
								if ((UInt8)(*buf) <= 0x7F) //Ӣ��˫�ַ���
								{
									FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 1);
								}
								else //���ķ���
								{
									FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
								}
							}							
						}
					}
					MemPtrFree(buf);
					//�򿪽��ռ�����Ϣ
					SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}	
				else if ((curKey == pref->MBSwitchKey || (curKey == pref->TempMBSwitchKey && !(pref->activeStatus & tempMBSwitchMask))) && !pref->LongPressMBSwich)//�̰�������л�
				{
					//���ν��ռ�����Ϣ
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
					//����¼�����
					ClearEvent(notifyPtr, ep);
					//������л�����
					MBSwitchEventHandler(pref, curKey != pref->MBSwitchKey);
					//�򿪽��ռ�����Ϣ
					SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				}
			}
			else if (pref->isLongPress || ((! pref->keyDownDetected) && (curModifiers & poweredOnKeyMask)))	//�������Ѿ��������˵ģ�����δ��⵽���µļ�
			{
				pref->isLongPress = false;
				pref->longPressHandled = false;
				//����¼�����
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
			//����¼�����
			ClearEvent(notifyPtr, ep);
		}
		else if (curKeyCode == pref->IMESwitchKey)
		{ //��Ӣ���л������л�������״̬���������뷨
			//����¼�����
			ClearEvent(notifyPtr, ep);
			//���״̬���
			pref->activeStatus &= (~tempDisabledMask);
			//��¼���뷨״̬
			pref->last_mode = imeModeChinese;
			if (pref->init_mode == initRememberFav)
			{
				SetInitModeOfField(pref);
			}
			//���ù����ɫ�������ظ���
			SetKeyRates(false, pref);
			SetCaretColor(false, pref);
			if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask))  //Java��DTGģʽ����������ʾģʽ����ʾ״̬����
			{										
				if(pref->javaStatusStyle == Style1)
				{
					SLWinDrawBitmap(NULL, bmpChIcon, 19,19, false);
					//WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
					//WinDrawChars("��", 2, 150, 148);
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
//���ü����¼�����
static void ExtKeyboardEventHandler(SysNotifyParamType *notifyPtr, EventType *ep, WChar curKey, UInt16 curKeyCode, UInt16 curModifiers, stru_Pref *pref)
{
	Char			*buf;
	//UInt8			operation;
	UInt16			txtlen=0;
	UInt16			cardNo;
	LocalID			dbID;
	RGBColorType	prevRgbP;
	//Int x,y;

	if (! (pref->activeStatus & tempDisabledMask)) //����״̬
	{
		if (curKey >= pref->keyRange[0] && curKey <= pref->keyRange[1] && StrChr(pref->curMBInfo.used_char, curKey) != NULL)
		{ //�����ǿ��Դ���ģ��������
			//����¼�����
			ClearEvent(notifyPtr, ep);
			//���ν��ռ�����Ϣ
			SysCurAppDatabase(&cardNo, &dbID);
			SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
			Output(curKey, pref);//��׼ģʽ	
			//�򿪽��ܼ�����Ϣ
			SysNotifyRegister(cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
		}
		else if (curKey == pref->IMESwitchKey)
		{ //��Ӣ���л������л���Ӣ��״̬����ʱ�ر����뷨
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
					//WinDrawChars("Ӣ", 2, 150, 148);
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
		{ //�л����
			//���ν��ռ�����Ϣ
			SysCurAppDatabase(&cardNo, &dbID);
			SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
			//����¼�����
			ClearEvent(notifyPtr, ep);
			//������л�����
			MBSwitchEventHandler(pref, curKey != pref->KBMBSwitchKey);
			//�򿪽��ռ�����Ϣ
			SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
		}
		else if (curKey == pref->PuncKey || curKey == pref->ListKey)
		{ //�����̻�����Ϣ
			//���ν��ռ�����Ϣ
			SysCurAppDatabase(&cardNo, &dbID);
			SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
			//����¼�����
			ClearEvent(notifyPtr, ep);
			buf = (Char *)MemPtrNew(15);
			MemSet(buf, 15, 0x00);
			//�򿪷����̻�����Ϣ
			(curKey == pref->PuncKey) ? PuncTrayEventHandler(buf, &txtlen, pref) : AltEventHandler(buf, &txtlen, pref);
			if(txtlen)//�н��
			{
				if ((pref->activeStatus & inJavaMask))
				{
					EnqueueResultToKey(buf, txtlen);
				}
				else if ((FldGetMaxChars(pref->current_field) - FldGetTextLength(pref->current_field)) > txtlen)
				{
					FldInsert(pref->current_field, buf, txtlen);
					if (curKey == pref->PuncKey) //���ַ��Ź�궨λ���м�
					{
						if (((UInt8)(*buf) > 0x7F && StrLen(buf) == 4 && buf[0]==buf[2] && (UInt8)buf[3]-(UInt8)buf[1]==1) ||
							((UInt8)(*buf) <= 0x7F && StrLen(buf) == 2))
						{
							if ((UInt8)(*buf) <= 0x7F) //Ӣ��˫�ַ���
							{
								FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 1);
							}
							else //���ķ���
							{
								FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
							}
						}
					}
				}
			}
			MemPtrFree(buf);
			//�򿪽��ռ�����Ϣ
			SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
		}
		else
		{ //������
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
					if (StrLen(buf) > 2 && (StrCompare(buf, "����") != 0 && StrCompare(buf, "����") != 0))
					{
						FldSetInsPtPosition(pref->current_field, FldGetInsPtPosition(pref->current_field) - 2);
					}
				}
			}
			MemPtrFree(buf);
		}
	}
	else if (curKey == pref->IMESwitchKey)
	{ //��Ӣ���л������л�������״̬���������뷨
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
				//WinDrawChars("��", 2, 150, 148);
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

//---------------------����ģ��----------------------------------------------------------------
//ȡ������ı���
static FieldType *GetActiveField(stru_Pref *pref)
{
	FormType	*curForm;
	UInt16		curObject;
	TableType	*curTable;
	
	curForm = FrmGetActiveForm(); //ȡ��ǰ�����
	if (curForm) //�������
	{
		UInt16 id=1200;
		curObject = FrmGetFocus(curForm); //ȡ��ý���Ķ���
		if (curObject != noFocus) //��ǰ����߱�����
		{
			if (FrmGetObjectType(curForm, curObject) == frmFieldObj) //��ͨ�ı���
			{
				pref->field_in_table = false;
				return FrmGetObjectPtr(curForm, curObject);
			}
			else if (FrmGetObjectType(curForm, curObject) == frmTableObj) //����е��ı���
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
//��⵱ǰ�����Ƿ�߱�����
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
//�������뷨
static void ActiveIME(stru_Pref *pref)
{
	pref->Actived = true;
	pref->hasShiftMask = false;
	pref->hasOptionMask = false;
	pref->isLongPress = false;
	pref->longPressHandled = false;
	switch (pref->init_mode)
	{
		case initDefaultChinese: //Ĭ������
		{
			pref->activeStatus &= (~tempDisabledMask);
			SetKeyRates(false, pref);
			SetCaretColor(false, pref);
			pref->last_mode = imeModeChinese;
			break;
		}
		case initDefaultEnglish: //Ĭ��Ӣ��
		{
			pref->activeStatus |= tempDisabledMask;
			SetKeyRates(true, pref);
			SetCaretColor(true, pref);
			pref->last_mode = imeModeEnglish;
			break;
		}
		case initKeepLast: //���״̬
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
		case initRememberFav: //��ס״̬
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
//��������
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
				case sysNotifyEventDequeuedEvent: //�¼�����֪ͨ
				{
					pref = (stru_Pref *)notifyPtr->userDataP;
					ep = (EventType *)notifyPtr->notifyDetailsP;
					switch (ep->eType)
					{
						case NativeFldEnterEvent: //�����ı������
						{
							FieldType	*theField = GetActiveField(pref);
							if (pref->current_field != theField)
							{
								pref->current_field = theField;
								ActiveIME(pref);
							}
							break;
						}
						case NativeKeyDownEvent: //��������
						{
							//��ȡ����������
							curKey = CharToLower(ByteSwap16(ep->data.keyDown.chr));
							curKeyCode = ByteSwap16(ep->data.keyDown.keyCode);
							curModifiers = ByteSwap16(ep->data.keyDown.modifiers);
							//��ȡ����ı���
							current_field = GetActiveField(pref);
							if (current_field) //�л���ı���
							{
								pref->current_field = current_field;
								if ((! pref->Actived) || current_field != pref->current_field) //��ǿģʽ���ã����ı���ı䣬�������뷨
								{
									ActiveIME(pref);
								}
								if (isVaildWindow()) //����Ϸ���������һ��From�Ĵ��壬������һ��������window������������һ��menu
								{
									if (((! GrfLocked(pref)) || curKey == pref->IMESwitchKey))
									{ //��д����������δ�򿪣����µ������뷨�л���
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
								{ //�ڵ�����window�м�⵽�ܼ��������İ�������������menu�¼����Ѱ������Ϊ�Ѵ������������
									pref->keyDownDetected = false;
								}
							}
							else if (pref->DTGSupport) //û�л���ı��򣬵�DTG��Java֧�ֱ����ã�ת��DTG��Java����
							{
								if ((pref->isTreo && hasOptionPressed(curModifiers, pref) && curKeyCode == pref->JavaActiveKey) ||
									((! pref->isTreo) && curKey == pref->JavaActiveKey))//DTGģʽ�����ر�
								{
									if (curModifiers & willSendUpKeyMask)
									{
										//����¼�
										ClearEvent(notifyPtr, ep);
									}
									else
									{
										if (pref->Actived) //�Ѿ�����ر�
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
										else //δ�����
										{
											//����¼���Ϣ
											ClearEvent(notifyPtr, ep);
											pref->Actived = true;
											pref->hasShiftMask = false;
											pref->hasOptionMask = false;
											pref->isLongPress = false;
											pref->activeStatus &= (~tempDisabledMask);
											pref->activeStatus |= inJavaMask; //��java״̬
											pref->curWin = NULL;
											pref->current_field = NULL;
										}
										ClearGrfState(pref);
									}
								}
								else if ((pref->activeStatus & inJavaMask)) //�������
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
							else if (pref->Actived) //û�л�ı���DTGδ���ã����뷨���ʹ������
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
							if (pref->Actived) //���뷨�Ѽ���
							{
								if (((InsPtEnabled ()) && (!pref->onlyJavaModeShow))||(pref->activeStatus & inJavaMask)) //������Java��DTG�У�����״̬ͼ��
								{
									if (isVaildWindow())
									{
										if ((pref->activeStatus & tempDisabledMask)) //Ӣ��״̬
										{
											if(pref->javaStatusStyle == Style1)
											{
												SLWinDrawBitmap(NULL, bmpEnIcon, 19,19, false);
												//WinSetForeColorRGB (&pref->englishStatusColor, &prevRgbP);
												//WinDrawChars("Ӣ", 2, 150, 148);
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
										else //����״̬
										{											
											if(pref->javaStatusStyle == Style1)
											{
												//DrawChIcon();
												SLWinDrawBitmap(NULL, bmpChIcon, 19,19, false);
												//WinSetForeColorRGB (&pref->chineseStatusColor, &prevRgbP);
												//WinDrawChars("��", 2, 150, 148);
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
								else if (GetActiveField(pref) == NULL) //û�л���ı������뷨����
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
				case sysNotifyInsPtEnableEvent: //���״̬֪ͨ
				{
					pref = (stru_Pref *)notifyPtr->userDataP;
					if ((! pref->Actived) && (*(Boolean *)notifyPtr->notifyDetailsP)) //��걻����������뷨
					{
						pref->current_field = GetActiveField(pref);
						ActiveIME(pref);
					}
					break;
				}
				case sysNotifyVolumeMountedEvent: //���濨��Ч�������������޷���ȡ���ʱ�Ż�ע�᱾��Ϣ���ȴ�ϵͳװ�ش��濨
				{
					pref = (stru_Pref *)notifyPtr->userDataP;
					//ȡ���Ը���Ϣ��ע�ᣬȷ��ֻ����һ��
					SysCurAppDatabase(&cardNo, &dbID);
					SysNotifyUnregister(cardNo, dbID, sysNotifyVolumeMountedEvent, sysNotifyNormalPriority);
					//���Ի�ȡ�����Ϣ
					if (GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load))
					{ //�ɹ����������뷨
						MemPtrSetOwner(pref, 0);
						prefAddress = (UInt32)pref;
						FtrSet(appFileCreator, ftrPrefNum, prefAddress);
						MemPtrSetOwner(pref, 0);
						SysNotifyRegister (cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
						SysNotifyRegister (cardNo, dbID, sysNotifyInsPtEnableEvent, NULL, sysNotifyNormalPriority, pref);
					}
					else
					{ //ʧ�ܣ��ر����뷨
						pref->Enabled = false;
						PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
						MemPtrFree(pref);
					}
					break;
				}
			}
			break;
		}
		case sysAppLaunchCmdSystemReset: //�����Զ�����
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
//---------------------����ģ��----------------------------------------------------------------
//�Ѱ���ת����Сд��ĸ
static WChar CharToLower(WChar key)
{
	if (key >= 'A' && key <= 'Z')
	{
		key += 32;
	}
	
	return key;
}
//��ȡ�Զ���������ֵת����ģ�����б�
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
		//�����
		if (mb_loaded || mb_info->inRAM)
		{
			db_ref = DmOpenDatabaseByTypeCreator(mb_info->db_type, 'pIME', dmModeReadOnly);
			if (db_ref != NULL)
			{
				//ȡ��Ϣ��¼
				record_handle = DmQueryRecord(db_ref, 0);
			}
			else
			{
				return false;
			}
		}
		else
		{
			//ȡ��ָ��
			while (vol_iterator != vfsIteratorStop)
			{
				VFSVolumeEnumerate(&vol_ref, &vol_iterator);
			}
			full_path = (Char *)MemPtrNew(100);
			//��������·��
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
		//���ģ������Ϣ
		for (i = 0; i < 11; i ++)
		{
			MemSet(mb_info->blur_head[i].key1, 5, 0x00);
			MemSet(mb_info->blur_head[i].key2, 5, 0x00);
			MemSet(mb_info->blur_tail[i].key1, 5, 0x00);
			MemSet(mb_info->blur_tail[i].key2, 5, 0x00);
			mb_info->blur_head[i].actived = false;
			mb_info->blur_tail[i].actived = false;
		}
		record_save = record; //����ԭʼ��¼ƫ����
		//ȡ�Զ�������Ϣ
		if (mb_info->syncopate_offset > 0)
		{
			record += mb_info->syncopate_offset; //ƫ�Ƶ��Զ�����
			mb_info->key_syncopate = MemPtrNew(mb_info->syncopate_size); //�����ڴ�
			MemMove(mb_info->key_syncopate, record, mb_info->syncopate_size); //ȡ��Ϣ
			MemPtrSetOwner(mb_info->key_syncopate, 0);
			record = record_save; //�ָ�ԭʼƫ����
		}
		//ȡ��ֵת����Ϣ
		if (mb_info->translate_offset > 0)
		{
			record += mb_info->translate_offset; //ƫ�Ƶ���ֵת��
			mb_info->key_translate = MemPtrNew(mb_info->translate_size); //�����ڴ�
			MemMove(mb_info->key_translate, record, mb_info->translate_size); //ȡ��Ϣ
			MemPtrSetOwner(mb_info->key_translate, 0);
			record = record_save; //�ָ�ԭʼƫ����
		}
		//ȡģ������Ϣ
		record += mb_info->smart_offset; //ƫ�Ƶ�ģ����
		head_sp = 0;
		tail_sp = 0;
		while (read_size < mb_info->smart_size) //ѭ����ģ������β
		{
			if (*record == '<') //ǰģ����
			{
				record ++; //��������1
				read_size ++;
				//���ָ���
				i = 0;
				while (record[i] != '-' && record[i] != '=')
				{
					i ++;
				}
				//���������Ϣ
				if(record[i] == '=' || ((! ActivedOnly) && record[i] == '-')) //Ӧ�ö�ȡ����
				{
					if (record[i] == '=')
					{
						mb_info->blur_head[head_sp].actived = true;
					}
					StrNCopy(mb_info->blur_head[head_sp].key1, record, i);
					record += i + 1; //��������2
					read_size += i + 1;
					i = 0;
					//��������
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					StrNCopy(mb_info->blur_head[head_sp].key2, record, i);
					record += i + 1; //��������һ����
					read_size += i + 1;
					head_sp ++;
				}
				else //��������
				{
					record += i + 1; //��������2
					read_size += i + 1;
					i = 0;
					//��������
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					record += i + 1; //��������һ����
					read_size += i + 1;
				}
			}
			else //��ģ����
			{
				record ++; //��������1
				read_size ++;
				//���ָ���
				i = 0;
				while (record[i] != '-' && record[i] != '=')
				{
					i ++;
				}
				//���������Ϣ
				if(record[i] == '=' || ((! ActivedOnly) && record[i] == '-')) //Ӧ�ö�ȡ����
				{
					if (record[i] == '=')
					{
						mb_info->blur_tail[tail_sp].actived = true;
					}
					StrNCopy(mb_info->blur_tail[tail_sp].key1, record, i);
						record += i + 1; //��������2
					read_size += i + 1;
					i = 0;
					//��������
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					StrNCopy(mb_info->blur_tail[tail_sp].key2, record, i);
					record += i + 1; //��������һ����
					read_size += i + 1;
					tail_sp ++;
				}
				else //��������
				{
					record += i + 1; //��������2
					read_size += i + 1;
					i = 0;
					//��������
					while (record[i] != '\'' && record[i] != '\0')
					{
						i ++;
					}
					record += i + 1; //��������һ����
					read_size += i + 1;
				}
			}
		}
		//�ͷż�¼���ر����
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
//�������Ƿ����޸Ĺ������У�����޸Ĺ��ı�ǲ������棻���򷵻ؼ�
static Boolean MBModified(stru_MBInfo *mb_info)
{
	DmOpenRef		db_ref;
	UInt16			i;
	UInt16			attr;
	Boolean			modified = false;
	
	//�����ݿ�
	db_ref = DmOpenDatabaseByTypeCreator(mb_info->db_type, 'pIME', dmModeReadWrite);
	//ѭ�����м�¼
	for (i = 0; i < 703; i ++)
	{
		//��ȡ��¼��ϸ����Ϣ
		DmRecordInfo(db_ref, i, &attr, NULL, NULL);
		if ((attr & 0x40)) //dirty��־���ڣ�����޸Ĺ�
		{
			modified = true;
			//�����ñ��
			attr = (attr & 0xFFBF);
			DmSetRecordInfo(db_ref, i, &attr, NULL);
		}
	}
	//�ر����ݿ�
	DmCloseDatabase(db_ref);
	
	return modified;
}
//--------------------------------------------------------------------------
//���Ҫ�����������ڴ����Ƿ���ڣ������ڣ�������
static Boolean MBExistInRAM(Char *full_path)
{
	UInt16			vol_ref;
	UInt32			vol_iterator = vfsIteratorStart;
	UInt32			dir_iterator = vfsIteratorStart;
	UInt32			db_type;
	Boolean			mb_exist_in_ram = false;
	FileRef			file_ref;
	DmOpenRef		db_ref;
	
	//ȡ��ָ��
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref != 0) //������
	{
		if (VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref) == errNone) //�����
		{
			//ƫ�Ƶ��������
			VFSFileSeek(file_ref, vfsOriginBeginning, 60);
			//��ȡ�������
			VFSFileRead (file_ref, 4, &db_type, NULL);
			//�ر����
			VFSFileClose(file_ref);
			db_ref = DmOpenDatabaseByTypeCreator(db_type, 'pIME', dmModeReadOnly);
			if (DmGetLastErr() == errNone) //����Ѿ�����
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
//װ�ػ�ж�´��濨�ϵ����
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
	
	//ȡ��ָ��
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref > 0) //������
	{
		switch (op)
		{
			case LOAD: //����
			{
				//��������·��
				full_path = (Char *)MemPtrNew(StrLen(mb_info->file_name) + 27);
				StrCopy(full_path, PIME_CARD_PATH);
				StrCat(full_path, mb_info->file_name);
				if (! MBExistInRAM(full_path)) //������ڴ��в�����
				{
					if (show_status)
					{
						//��ʾ������ʾ����
						frmP = FrmInitForm(frmLoadMB);
						FrmSetActiveForm(frmP);
						FrmDrawForm(frmP);
					}
					else
					{
						//���������ʾ
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
						WinDrawChars("�������....", 12, 20, 23);
					}
					//�������
					VFSImportDatabaseFromFile(vol_ref, full_path, &card_no, &db_id);
					if (show_status)
					{
						//�ر�������ʾ
						FrmReturnToForm(0);
					}
					else
					{
						//�ָ���ͼ����
						WinRestoreBits(save_win, 6, 18);
					}
				}
				//�ͷ��ڴ�
				MemPtrFree(full_path);
				break;
			}
			case SAVE: //ж��
			{
				if (show_status)
				{
					//��ʾж����ʾ
					frmP = FrmInitForm(frmSaveMB);
					FrmSetActiveForm(frmP);
					FrmDrawForm(frmP);
				}
				else
				{
					//���ж����ʾ
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
					WinDrawChars("�������....", 12, 20, 23);
				}
				//��������·��
				full_path = (Char *)MemPtrNew(StrLen(mb_info->file_name) + 27);
				StrCopy(full_path, PIME_CARD_PATH);
				StrCat(full_path, mb_info->file_name);
				//ȡ�����Ϣ
				DmGetNextDatabaseByTypeCreator(true, &stateInfo, mb_info->db_type, 'pIME', true, &card_no, &db_id);
				//ɾ�����ϵľɰ汾���
				VFSFileDelete(vol_ref, full_path);
				//�����°汾���
				VFSExportDatabaseToFile(vol_ref, full_path, card_no, db_id);
				//�ͷ��ڴ�
				MemPtrFree(full_path);
				if (show_status)
				{
					//�ر�ж����ʾ
					FrmReturnToForm(0);
				}
				else
				{
					//�ָ���ͼ����
					WinRestoreBits(save_win, 6, 18);
				}
				break;
			}
		}
	}
}
//--------------------------------------------------------------------------
//�������ڴ�Ĵ��濨����沢���ڴ����Ƴ�
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
	
	//�������Ϣ���ݿ�
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
	if (! DmGetLastErr())
	{
		mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
		if (mb_num > 0)
		{
			//����װ�ص��ڴ��еĴ��濨���
			for (i = start; i < mb_num; i ++)
			{
				record_handle = DmQueryRecord(dbRef, i);
				mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
				if (! mb_list_unit->inRAM)
				{
					mb_info.db_type = mb_list_unit->MBDbType;
					if (DmGetNextDatabaseByTypeCreator(true, &stateInfo, mb_info.db_type, 'pIME', true, &card_no, &db_id) == errNone) //����
					{
						if (MBModified(&mb_info)) //����޸Ĺ����ȱ��浽����
						{
							StrCopy(mb_info.file_name, mb_list_unit->file_name);
							//���浽����
							SaveLoadMB(&mb_info, SAVE, show_status);
						}
						//���ڴ���ɾ��
						DmDeleteDatabase(card_no, db_id);
					}
				}
				MemHandleUnlock(record_handle);
			}
		}
		DmCloseDatabase(dbRef);
	}
}
//���������Ϣ
static void SetMBInfoByNameType(Char *file_name, UInt32 db_type, Boolean inRAM, stru_MBInfo *mb_info)
{
	Char		*record;
	DmOpenRef	db_ref;
	MemHandle	record_handle;
	
	//�����
	db_ref = DmOpenDatabaseByTypeCreator(db_type, 'pIME', dmModeReadWrite);
	//ȡ��Ϣ��¼
	record_handle = DmGetRecord(db_ref, 0);
	record = (Char *)MemHandleLock(record_handle); 
	//������Ϣ
	DmWrite(record, 0, mb_info->name, 9); //�������
	DmWrite(record, 9, &mb_info->type, 1); //�������
	DmWrite(record, 10, &mb_info->index_offset, 4); //����ƫ����
	DmWrite(record, 14, &mb_info->key_length, 1); //�볤
	DmWrite(record, 15, mb_info->used_char, 30); //��ֵ��Χ
	DmWrite(record, 45, &mb_info->wild_char, 1); //���ܼ�
	DmWrite(record, 46, &mb_info->syncopate_offset, 4); //ȫ��Ԫƫ����
	DmWrite(record, 50, &mb_info->syncopate_size, 4); //ȫ��Ԫ�ߴ�
	DmWrite(record, 54, &mb_info->translate_offset, 4); //��ֵת��ƫ����
	DmWrite(record, 58, &mb_info->translate_size, 4); //��ֵת���ߴ�
	DmWrite(record, 62, &mb_info->smart_offset, 4); //ģ����ƫ����
	DmWrite(record, 66, &mb_info->smart_size, 4); //ģ����ߴ�
	//������Ϣ
	DmWrite(record, 91, &mb_info->gradually_search, 1); //��������
	DmWrite(record, 92, &mb_info->frequency_adjust, 1); //��Ƶ����
	//�ر����
	MemHandleUnlock(record_handle);
	DmReleaseRecord(db_ref, 0, true);
	DmCloseDatabase(db_ref);
}
//--------------------------------------------------------------------------
//��ȡ�����Ϣ
static void GetMBInfoByNameType(Char *file_name, UInt32 db_type, Boolean inRAM, stru_MBInfo *mb_info)
{
	Char		*record;
	DmOpenRef	db_ref;
	MemHandle	record_handle;
	UInt16		vol_ref;
	UInt32		vol_iterator = vfsIteratorStart;
	FileRef		file_ref = 0;
	Char		*full_path;
	
	//�����
	db_ref = DmOpenDatabaseByTypeCreator(db_type, 'pIME', dmModeReadOnly);
	if (DmGetLastErr() != errNone)
	{
		//ȡ��ָ��
		while (vol_iterator != vfsIteratorStop)
		{
			VFSVolumeEnumerate(&vol_ref, &vol_iterator);
		}
		full_path = (Char *)MemPtrNew(100);
		//��������·��
		StrCopy(full_path, PIME_CARD_PATH);
		StrCat(full_path, file_name);
		VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref);
		MemPtrFree(full_path);
		VFSFileDBGetRecord(file_ref, 0, &record_handle, NULL, NULL);
	}
	else
	{
		//ȡ��Ϣ��¼
		record_handle = DmQueryRecord(db_ref, 0);
	}
	record = (Char *)MemHandleLock(record_handle); 
	//������Ϣ
	MemMove(mb_info->name, record, 9); //�������
	record += 9;
	mb_info->type = *(UInt8 *)record; //�������
	record ++;
	mb_info->index_offset = *(UInt32 *)record; //����ƫ����
	record += 4;
	mb_info->key_length = *(UInt8 *)record; //�볤
	record ++;
	MemMove(mb_info->used_char, record, 30); //��ֵ��Χ
	record += 30;
	mb_info->wild_char = *(Char *)record; //���ܼ�
	record ++;
	mb_info->syncopate_offset = *(UInt32 *)record; //ȫ��Ԫƫ����
	record += 4;
	mb_info->syncopate_size = *(UInt32 *)record; //ȫ��Ԫ�ߴ�
	record += 4;
	mb_info->translate_offset = *(UInt32 *)record; //��ֵת��ƫ����
	record += 4;
	mb_info->translate_size = *(UInt32 *)record; //��ֵת���ߴ�
	record += 4;
	mb_info->smart_offset = *(UInt32 *)record; //ģ����ƫ����
	record += 4;
	mb_info->smart_size = *(UInt32 *)record; //ģ����ߴ�
	//������Ϣ
	record += 25;
	mb_info->gradually_search = *(Boolean *)record; //��������
	record ++;
	mb_info->frequency_adjust = *(Boolean *)record; //��Ƶ����
	//�ر����
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
//ͨ�������������������Ϣ
static void SetMBInfoFormMBList(stru_MBInfo *mb_info, UInt16 mb_index)
{
	DmOpenRef	dbRef;
	stru_MBList	mb_list_unit;
	stru_MBList *record;
	UInt16		mb_num;
	MemHandle	record_handle;
	
	//�������Ϣ���ݿ�
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	if (mb_num > 0)
	{
		//ȡ��Ӧ�ļ�¼
		record_handle = DmGetRecord(dbRef, mb_index);
		record = (stru_MBList *)MemHandleLock(record_handle);
		//���������Ϣ���¼��Ϣ
		mb_list_unit.MBEnabled = mb_info->enabled;
		mb_list_unit.MBDbType = mb_info->db_type;
		mb_list_unit.inRAM = mb_info->inRAM;
		StrCopy(mb_list_unit.file_name, mb_info->file_name);
		DmWrite(record, 0, &mb_list_unit, sizeof(stru_MBList));
		//�ͷż�¼
		MemHandleUnlock(record_handle);
		DmReleaseRecord(dbRef, mb_index, true);
		//���õ�ǰ������Ϣ
		SetMBInfoByNameType(mb_info->file_name, mb_info->db_type, mb_info->inRAM, mb_info);
	}
	DmCloseDatabase(dbRef);
}
//--------------------------------------------------------------------------*/
//ͨ�����������ȡ������Ϣ���������ڿ��ϣ��������ڴ棬����ԭ���ӿ������������������濨
static void GetMBInfoFormMBList(stru_MBInfo *mb_info, UInt16 mb_index, Boolean show_status, Boolean need_load_mb)
{
	DmOpenRef			dbRef;
	stru_MBList			*mb_list_unit;
	UInt16				mb_num;
	MemHandle			record_handle;
	
	//�������Ϣ���ݿ�
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	if (mb_num > 0)
	{
		//ȡ��Ӧ�ļ�¼
		record_handle = DmQueryRecord(dbRef, mb_index);
		mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
		//����ͬһ�������Ҫ����װж����
		if (mb_info->db_type != mb_list_unit->MBDbType)
		{
			//����װ�ص��ڴ��еĴ��濨���
			UnloadMB(unloadAll, show_status);
			//�ͷ��ڴ�
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
			if ((! mb_info->inRAM) && need_load_mb) //�ڿ��ϣ������ڴ�
			{
				SaveLoadMB(mb_info, LOAD, show_status);
			}
			//��ȡ��ǰ������Ϣ
			GetMBInfoByNameType(mb_info->file_name, mb_info->db_type, mb_info->inRAM, mb_info);
		}
		//�ͷż�¼
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
//ͨ�������������Ϣ��ļ�¼�ţ����ظü�¼�м�¼������Ƿ��Ѿ�����
static Boolean MBEnabled(UInt16 mb_index)
{
	DmOpenRef		dbRef;
	stru_MBList		*mb_list_unit;
	MemHandle		record_handle;
	Boolean			mb_enabled;
	
	//�������Ϣ���ݿ�
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadOnly);
	//ȡ��Ӧ�ļ�¼
	record_handle = DmQueryRecord(dbRef, mb_index);
	mb_list_unit = (stru_MBList *)MemHandleLock(record_handle);
	mb_enabled = mb_list_unit->MBEnabled;
	//�ͷż�¼
	MemHandleUnlock(record_handle);
	//�ر����ݿ�
	DmCloseDatabase(dbRef);
	
	return mb_enabled;
}
#pragma mark -
//--------------------------------------------------------------------------
//���option���Ƿ񱻰���
static Boolean hasOptionPressed(UInt16 modifiers, stru_Pref *pref)
{
	Boolean capsLock		= false;
	Boolean	numLock			= false;
	Boolean optLock			= false;
	Boolean autoShifted		= false;
	Boolean	optionPressed	= false;
	UInt16	tempShift		= 0;
	
	if ((modifiers & optionKeyMask)) //����option����
	{
		optionPressed = true;
	}
	else
	{
		if (pref->isTreo) //HSϵ��״̬����
		{
			HsGrfGetStateExt(&capsLock, &numLock, &optLock, &tempShift, &autoShifted);
		}
		else //��׼״̬����
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
//���û�ָ������ɫ
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
//���û�ָ������ӳ�
static void SetKeyRates(Boolean reset, stru_Pref *pref)
{
	UInt16 initDelayP;
	UInt16 periodP;
	UInt16 doubleTapDelayP;
	Boolean queueAheadP;
	
	if (! reset) //���óɿ���
	{
		KeyRates(false, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
		initDelayP = (SysTicksPerSecond() >> 2);
		KeyRates(true, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
	}
	else //�ָ���Ĭ���ٶ�
	{
		KeyRates(false, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
		initDelayP = pref->defaultKeyRate;
		KeyRates(true, &initDelayP, &periodP, &doubleTapDelayP, &queueAheadP);
	}
}
//--------------------------------------------------------------------------
//���浱ǰ�ı���״̬
static void SetInitModeOfField(stru_Pref *pref)
{
	MemHandle		record_handle;
	DmOpenRef		db_ref;
	
	if ((! (pref->activeStatus & inJavaMask)) && pref->current_field != NULL) //״̬�Ϸ������Ա���
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
//��ȡ��¼�е��ı���״̬
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
	
	if (pref->current_field != NULL) //�ı���ָ��Ϸ�
	{
		form = FrmGetActiveForm(); //��ȡ��ǰ����
		if (form != NULL) //����ָ��Ϸ�
		{
			//��ȡ��ǰ��Ϣ
			init_info.form_id = FrmGetFormId(form); //����ID
			init_info.object_count = FrmGetNumberOfObjects(form); //����Ŀؼ���
			if (pref->field_in_table) //����е��ı����޷�ȡID��ȡ����ID
			{
				i = FrmGetFocus(form);
				if (FrmGetObjectType(form, i) == frmTableObj) //ȷʵ�Ǳ��
				{
					init_info.field_id = FrmGetObjectId(form, i); //���ID
				}
				else //��֪����ʲô���˳�����
				{
					return imeModeChinese;
				}
			}
			else
			{
				init_info.field_id = FrmGetObjectId(form, FrmGetObjectIndexFromPtr(form, pref->current_field)); //�ı���ID
			}
			//�����ݿ⣬���бȽ�
			db_ref = DmOpenDatabaseByTypeCreator('init', appFileCreator, dmModeReadWrite);
			record_count = DmNumRecords(db_ref);
			i = 0;
			while (i < record_count && not_found)
			{
				record_handle = DmQueryRecord(db_ref, i);
				record = (stru_InitInfo *)MemHandleLock(record_handle);
				if (MemCmp(&init_info, record, 6) == 0) //�ҵ����
				{
					init_info.mode = record->mode;
					pref->init_mode_record = i;
					not_found = false;
				}
				MemHandleUnlock(record_handle);
				i ++;
			}
			//��û���ҵ�ƥ�����Ϣ��������Ϣ���浽���ݿ�
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
			//�ر����ݿ�
			DmCloseDatabase(db_ref);
		}
	}
	
	return init_info.mode;
}
//--------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////////////////

//---------------------����ģ��----------------------------------------------------------------
//����ģ�����б�
static void UpdateBlurList(ListType *lstP, stru_MBInfo *mb_info, Char ***blur_list, UInt16 *blur_num)
{
	UInt16		i;
	UInt16		j;
	Int16		lst_selection;
	
	//�����б�״̬
	lst_selection = LstGetSelection(lstP);
	//����ɵ��б�
	if ((*blur_num) > 0)
	{
		for (i = 0; i < (*blur_num); i ++)
		{
			MemPtrFree((*blur_list)[i]);
		}
		MemPtrFree((*blur_list));
		(*blur_num) = 0;
	}
	//��ȡģ����������
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
	//�������б�
	(*blur_list) = (Char **)MemPtrNew(((*blur_num) << 2));
	i = 0;
	//ǰģ����
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
	//��ģ����
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
	//�����б�
	LstSetListChoices(lstP, (*blur_list), (*blur_num));
	LstDrawList(lstP);
	LstSetSelection(lstP, lst_selection);
}
//--------------------------------------------------------------------------
//��ͣģ����
static void SwitchBlurActiveStatus(stru_MBInfo *mb_info, UInt16 blur_num, UInt16 blur_index)
{
	UInt16		i = 0;
	UInt16		j = 0;
	UInt32		write_offset;
	Boolean		matched = false;
	Char		*record;
	MemHandle	record_handle;
	DmOpenRef	db_ref;
	
	//������Ҫ������ģ����
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
	//����ģ����
	//�����
	db_ref = DmOpenDatabaseByTypeCreator(mb_info->db_type, 'pIME', dmModeReadWrite);
	//��ȡ��¼
	record_handle = DmGetRecord(db_ref, 0);
	record = (Char *)MemHandleLock(record_handle);
	//д��ģ����
	write_offset = mb_info->smart_offset;
	//ǰģ����
	i = 0;
	while (mb_info->blur_head[i].key1[0] != '\0')
	{
		DmWrite(record, write_offset, "<", 1);
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_head[i].key1, StrLen(mb_info->blur_head[i].key1)); //��1
		write_offset += StrLen(mb_info->blur_head[i].key1);
		if (mb_info->blur_head[i].actived) //��ͣ����
		{
			DmWrite(record, write_offset, "=", 1);
		}
		else
		{
			DmWrite(record, write_offset, "-", 1);
		}
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_head[i].key2, StrLen(mb_info->blur_head[i].key2)); //��2
		write_offset += StrLen(mb_info->blur_head[i].key2);
		if (write_offset + 1 < mb_info->smart_size) //�ָ������
		{
			DmWrite(record, write_offset, "\'", 1);
		}
		write_offset ++;
		i ++;
	}
	//��ģ����
	i = 0;
	while (mb_info->blur_tail[i].key1[0] != '\0')
	{
		DmWrite(record, write_offset, ">", 1);
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_tail[i].key1, StrLen(mb_info->blur_tail[i].key1)); //��1
		write_offset += StrLen(mb_info->blur_tail[i].key1);
		if (mb_info->blur_tail[i].actived) //��ͣ����
		{
			DmWrite(record, write_offset, "=", 1);
		}
		else
		{
			DmWrite(record, write_offset, "-", 1);
		}
		write_offset ++;
		DmWrite(record, write_offset, mb_info->blur_tail[i].key2, StrLen(mb_info->blur_tail[i].key2)); //��2
		write_offset += StrLen(mb_info->blur_tail[i].key2);
		if (write_offset + 1 < mb_info->smart_size) //�ָ������
		{
			DmWrite(record, write_offset, "\'", 1);
		}
		write_offset ++;
		i ++;
	}
	//�ͷż�¼
	MemHandleUnlock(record_handle);
	DmReleaseRecord(db_ref, 0, true);
	//�ر����
	DmCloseDatabase(db_ref);
}
//--------------------------------------------------------------------------
//����ģ����
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
	
	//��ģ�������ô���
	frmP = FrmInitForm(frmSetBlur);
	FrmDrawForm(frmP);
	FrmSetActiveForm(frmP);
	//��ȡ�����Ϣ
	MemSet(&mb_info, sizeof(stru_MBInfo), 0x00);
	GetMBInfoFormMBList(&mb_info, mb_index, true, true);
	//��ȡģ������Ϣ
	GetMBDetailInfo(&mb_info, false, true);
	//��ȡģ�����б�ָ��
	lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstBlur));
	//����ģ�����б�
	LstSetSelection(lstP, noListSelection);
	UpdateBlurList(lstP, &mb_info, &blur_list, &blur_num);
	
	//�¼�ѭ��
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		
		if (! SysHandleEvent(&event))
		{
			switch (event.eType)
			{
				case ctlSelectEvent:
				{
					if (event.data.ctlSelect.controlID == btnBOK) //�˳�
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
	
	//�ͷ��ڴ�
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
	//����
	FrmReturnToForm(0);
}
//--------------------------------------------------------------------------
//��������
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
	
	//�˳�����
	FrmReturnToForm(0);
	
	return CharToLower(key);
}
//--------------------------------------------------------------------------
//�߼����ý���
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
	
	//�¼�ѭ��
	do
	{
		EvtGetEvent(&event, evtWaitForever);
		//if (! SysHandleEvent(&event))
		//{
			switch (event.eType)
			{
				case frmLoadEvent:
				{
					//�򿪸߼����ô���
					frmP = FrmInitForm(frmAdvSetting);
			
					break;
				}
				case frmOpenEvent:
				{
					FrmDrawForm(frmP);
					FrmSetActiveForm(frmP);
					
					lstP = FrmGetObjectPtr(frmP,FrmGetObjectIndex(frmP,lstNotifyPriority));//���ȼ�ѡ���б�ָ��
					triP = FrmGetObjectPtr(frmP,FrmGetObjectIndex(frmP,triggerNotifyPriority));//���ȼ�ѡ���б�����ָ��
					
					//��ʾ���ȼ��б�
					if ( pref->NotifyPriority == -128 ) PrioritySelection = 0;
					else if ( pref->NotifyPriority == -96 ) PrioritySelection = 1;
					else if ( pref->NotifyPriority == -64 ) PrioritySelection = 2;
					else if ( pref->NotifyPriority == -32 ) PrioritySelection = 3;
					else PrioritySelection = 4;
					
					LstSetSelection(lstP,PrioritySelection);
					CtlSetLabel(triP,LstGetSelectionText(lstP,LstGetSelection(lstP)));
					
					//��������ģʽ
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
					//�ڶ���ѡ�ּ��Ƿ�����
					for (i = 0; i < 5; i ++)
					{
						if (pref->Selector2[i] != 0)
						{
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cb2ndSelector), 1);
						}
					}
					
					//Java֧���Ƿ�����
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbEnableJava), pref->DTGSupport);
					
					//�Զ��л�����Ƿ�����
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbAutoMBSwich), pref->AutoMBSwich);
					
					//����л����ǳ������Ƕ̰�
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbLongPressMBSwich), pref->LongPressMBSwich);
					
					//��ʾGSIָʾ��
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbShowGsiButton), pref->showGsi);
					
					//��ʾ��ҳ��ť
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbChoiceButton), pref->choice_button);
					
					//��ʾ�˵���ť
					FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbMenuButton), pref->menu_button);
					
					//�̶������
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
						case btnRestore: //�ָ�Ĭ������
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
							//Ĭ�ϰ���
							pref->Selector[0] = 0x0020; pref->Selector[1] = keyZero; pref->Selector[2] = hsKeySymbol;
							pref->Selector[3] = keyLeftShift; pref->Selector[4] = keyRightShift;
							//Ĭ�ϰ���2
							pref->Selector2[0] = 0; pref->Selector2[1] = 0; pref->Selector2[2] = 0;
							pref->Selector2[3] = 0; pref->Selector2[4] = 0;
							//����ģʽ
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
							//���ⰴ��
							pref->IMESwitchKey = keyHard1;
							pref->JavaActiveKey = keySpace;
							pref->ListKey = hsKeySymbol;
							pref->KBMBSwitchKey = 0;
							pref->MBSwitchKey = 0;
							pref->TempMBSwitchKey = 0;
							pref->PuncKey = 0;
							pref->SyncopateKey = keyPeriod;
							pref->MenuKey = keyMenu;
							//�������뷨״̬
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
						case pbtnKBModeTreo: //Treo����ģʽ
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
						case pbtnKBModeXplore: //Ȩ�Ǽ���ģʽ
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
						case pbtnKBModeExt: //���ü���ģʽ
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
						case btnSetKBMBSwitchKey: //���ü���ģʽ-����л���
						{
							pref->KBMBSwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetPuncKey: //���ü���ģʽ-�����̼�
						{
							pref->PuncKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetListKey:
						{
							pref->ListKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}						
						case cbEnableJava: //��ͣJava��DTG֧��
						{
							pref->DTGSupport = ! pref->DTGSupport;
							if (pref->DTGSupport) //����
							{
								if (pref->Enabled)
								{
									SysCurAppDatabase(&cardNo, &dbID);
									SysNotifyRegister (cardNo, dbID, sysNotifyVirtualCharHandlingEvent, NULL, sysNotifyNormalPriority, pref);
								}
							}
							else //�ر�
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
						case cbAutoMBSwich: //��ͣ�Զ��л����
						{
							pref->AutoMBSwich = ! pref->AutoMBSwich;
							break;
						}
						case cbLongPressMBSwich: //��ͣ�Զ��л����
						{
							pref->LongPressMBSwich = ! pref->LongPressMBSwich;
							break;
						}
						case btnSetSwitchKey: //�������뷨��ͣ��
						{
							pref->IMESwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetMenuKey: //���ò˵���
						{
							pref->MenuKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetMBSwitchKey: //�������뷨����л���
						{
							pref->MBSwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetTempMBSwitchKey: //�������뷨�����ʱ�л���
						{
							pref->TempMBSwitchKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}						
						case btnSetSyncopateKey: //����������
						{
							pref->SyncopateKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetJavaKey: //����Java��ͣ���л���
						{
							pref->JavaActiveKey = (UInt16)CustomKey(pref->KBMode, pref);
							break;
						}
						case btnSetKey1: //��һ��ѡ�ּ�
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
						case cbShowGsiButton: //��ʾGSIָʾ��
						{
							pref->showGsi = ! pref->showGsi;
							break;
						}
						case cbShowOnBottom: //�̶������
						{
							pref->shouldShowfloatBar = ! pref->shouldShowfloatBar;
							break;
						}
						case cbChoiceButton: //��ͣ��ҳ��ť
						{
							pref->choice_button = ! pref->choice_button;
							break;
						}
						case cbMenuButton: //��ͣ��ҳ��ť
						{
							pref->menu_button = ! pref->menu_button;
							break;
						}
						case btnExitAdvForm: //�˳��߼�����
						{
							exit = true;
							break;
						}
						case cb2ndSelector: //���õڶ��鰴��
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
						case btnSetKey21: //�ڶ���ѡ�ּ�
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
	
	//��������
	PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	//����
	FrmReturnToForm(0);
}
#pragma mark -
//--------------------------------------------------------------------------
//��ȡ���ϵ�����б�
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
	
	//ȡ��ָ��
	while (vol_iterator != vfsIteratorStop)
	{
		VFSVolumeEnumerate(&vol_ref, &vol_iterator);
	}
	if (vol_ref != 0) //������
	{
		if (VFSFileOpen(vol_ref, PIME_CARD_PATH, vfsModeReadWrite, &dir_ref) == errNone) //�� /PALM/Programs/PocketIME
		{
			file_info.nameP = (Char *)MemPtrNew(32);
			dir_iterator = vfsIteratorStart;
			//�����ļ���
			while (dir_iterator != vfsIteratorStop)
			{
				if (VFSDirEntryEnumerate(dir_ref, &dir_iterator, &file_info) == errNone)
				{
					file_ext_name = StrChr(file_info.nameP, (WChar)'.'); //ȡ��չ��
					if (file_ext_name != NULL) //��չ������
					{
						if (StrNCaselessCompare(file_ext_name, ".pdb", 4) == 0) //��.pdb����Ϊ����һ�����ݿ�
						{
							mb_num ++; //��������+1
						}
					}
				}
			}
			//�����ļ��б�
			(*mb_list_vfs) = (stru_MBList **)MemPtrNew((mb_num << 2));
			i = 0;
			dir_iterator = vfsIteratorStart;
			while (dir_iterator != vfsIteratorStop)
			{
				if (VFSDirEntryEnumerate(dir_ref, &dir_iterator, &file_info) == errNone)
				{
					file_ext_name = StrChr(file_info.nameP, (WChar)'.'); //ȡ��չ��
					if (file_ext_name != NULL) //��չ������
					{
						if (StrNCaselessCompare(file_ext_name, ".pdb", 4) == 0) //��.pdb����Ϊ����һ�����ݿ�
						{
							//�����б�Ԫ���ڴ�
							(*mb_list_vfs)[i] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList));
							MemSet((*mb_list_vfs)[i], sizeof(stru_MBList), 0x00);
							//�������
							StrCopy((*mb_list_vfs)[i]->file_name, file_info.nameP);
							i ++;
						}
					}
				}
			}
			VFSFileClose(dir_ref);
			MemPtrFree(file_info.nameP);
			//��ȡ�������
			full_path = (Char *)MemPtrNew(10240);
			for (i = 0; i < mb_num; i ++)
			{
				//��������·��
				StrCopy(full_path, PIME_CARD_PATH);
				StrCat(full_path, (*mb_list_vfs)[i]->file_name);
				//�����
				VFSFileOpen(vol_ref, full_path, vfsModeRead, &file_ref);
				//��ȡPDB����
				VFSFileSeek(file_ref, vfsOriginBeginning, 60);
				VFSFileRead(file_ref, 4, &(*mb_list_vfs)[i]->MBDbType, NULL);
				//�ر����
				VFSFileClose(file_ref);
			}
			MemPtrFree(full_path);
		}
		else // /PALM/Programs/PocketIME �����ڣ�������
		{
			VFSDirCreate(vol_ref, "/PALM/Programs/PocketIME");
		}
	}
	
	return mb_num;
}
//--
//�ж��Ƿ�Ϊ�������
static Boolean IsMBType(UInt32 type)
{
	return !(type == sysFileTApplication || type == sysFileTPanel || type == sysResTAppGData || type == 'init' || type == 'dict' || type == 'DAcc');
}
//--------------------------------------------------------------------------
//��ȡ�ڴ��е�����б�
static UInt16 GetMBListInRAM(stru_MBList ***mb_list)
{
	UInt16				mb_num;
	UInt16				db_count = 0;
	UInt16				i;
	UInt16				j;
	MemHandle			db_list_handle;
	SysDBListItemType	*db_list;
	
	if (SysCreateDataBaseList(0, appFileCreator, &db_count, &db_list_handle, false)) //ȡCreatorID='pIME'��ȫ�����ݿ�
	{
		if (db_count > 0)
		{
			db_list = (SysDBListItemType *)MemHandleLock(db_list_handle);
			//ȥ��type='appl'��'panl'��'data'�ļ���
			mb_num = db_count;
			for (i = 0; i < db_count; i ++)
			{
				if (!IsMBType(db_list[i].type))
				{
					mb_num --;
				}
			}
			//��������б�
			(*mb_list) = (stru_MBList **)MemPtrNew((mb_num << 2));
			j = 0;
			for (i = 0; i < db_count; i ++)
			{
				if (IsMBType(db_list[i].type))
				{
					(*mb_list)[j] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList)); //��������б�Ԫ���ڴ�
					MemSet((*mb_list)[j], sizeof(stru_MBList), 0x00);
					//�������
					(*mb_list)[j]->MBDbType = db_list[i].type;
					//������ڴ�
					(*mb_list)[j]->inRAM = true;
					//�������
					StrCopy((*mb_list)[j]->file_name, db_list[i].name);
					j ++;
				}
			}
			//�ͷ��ڴ�
			MemHandleUnlock(db_list_handle);
			MemHandleFree(db_list_handle);
		}
	}
	
	return mb_num;
}
//--------------------------------------------------------------------------
//���������Ϣ�Ⲣ��������б�
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
	
	//�������Ϣ��
	dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	if (DmGetLastErr()) //���ݿⲻ���ڣ��½�
	{
		DmCreateDatabase(0, "PIME_MBList", appFileCreator, 'data', false);
		dbRef = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	}
	//��ȡλ���ڴ������б�
	mb_num_ram = GetMBListInRAM(&mb_list_ram);
	//��ȡλ�ڴ��濨�ϵ�����б�
	mb_num_vfs = GetMBListOnVFS(&mb_list_vfs);
	//��ȡ�����Ϣ�������б������������ڵ����
	mb_num = DmNumRecordsInCategory(dbRef, dmAllCategories);
	//�����ڴ�
	(*mb_list) = (char **)MemPtrNew(80); //�б�
	mb_list_db = (stru_MBList **)MemPtrNew(80); //���
	if (mb_num > 0)
	{
		k = 0;
		for (i = 0; i < mb_num; i ++)
		{
			//ȡһ����¼
			record_handle = DmQueryRecord(dbRef, i);
			mb_unit = (stru_MBList *)MemHandleLock(record_handle);
			mb_exist = false;
			if (mb_unit->inRAM) //���ڴ��е�
			{
				//�Ƚ��ڴ�����б�
				for (j = 0; j < mb_num_ram; j ++)
				{
					if (mb_unit->MBDbType == mb_list_ram[j]->MBDbType) //����
					{
						MemSet(mb_list_ram[j]->file_name, 32, 0x00); //��Ǳ����������Ϣ���д���
						mb_exist = true;
						break;
					}
				}
			}
			else //�ڴ��濨�ϵ�
			{
				//�Ƚϴ��濨����б�
				for (j = 0; j < mb_num_vfs; j ++)
				{
					if (StrCompare(mb_unit->file_name, mb_list_vfs[j]->file_name) == 0)
					{
						MemSet(mb_list_vfs[j]->file_name, 32, 0x00); //��Ǳ����������Ϣ���д���
						mb_exist = true;
						break;
					}
				}
			}
			if (mb_exist) //������
			{
				UInt16 len=StrLen(mb_unit->file_name);
				if(mb_unit->file_name[len-4]=='.')
				{
					(*mb_list)[k] = (char *)MemPtrNew(len - 2);	
					StrNCopy((*mb_list)[k], mb_unit->file_name, len-4);
					(*mb_list)[k][len-4]=0x1a;//�������
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
		//ɾ�������Ϣ���е��б�
		for (i = 0; i < mb_num; i ++)
		{
			DmRemoveRecord(dbRef, 0);
		}
		//���������Ϣ����б���
		mb_num = k;
	}
	//����ڴ��е������
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
	//��Ӵ��濨�е������
	for (i = 0; i < mb_num_vfs; i ++)
	{
		if (mb_list_vfs[i]->file_name[0] != '\0')
		{
			UInt16 len;
			mb_num ++;
			len=StrLen(mb_list_vfs[i]->file_name);
			(*mb_list)[mb_num - 1] = (Char *)MemPtrNew(len - 2);
			StrNCopy((*mb_list)[mb_num - 1], mb_list_vfs[i]->file_name, len-4);
			(*mb_list)[mb_num - 1][len-4]=0x1a;//�������
			(*mb_list)[mb_num - 1][len-3]=0;
			mb_list_db[mb_num - 1] = (stru_MBList *)MemPtrNew(sizeof(stru_MBList));
			MemMove(mb_list_db[mb_num - 1], mb_list_vfs[i], sizeof(stru_MBList));
		}
	}
	MemPtrResize((*mb_list), (mb_num << 2));
	//д�������Ϣ��
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
	//�ͷ��ڴ�
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
	//�ر����ݿ�
	DmCloseDatabase(dbRef);
	
	return mb_num;
}
//--------------------------------------------------------------------------
//�ƶ������Ϣ�������¼
static void MoveMBRecordInMBListDB(UInt16 record_index, ListType *lstP, Char ***mb_list, UInt8 direction)
{
	DmOpenRef	db_ref;
	MemHandle	record_handle;
	stru_MBList	*record;
	UInt16		mb_num;
	UInt16		i;
	UInt16		obj_record_index;
	Int16		list_selection;
	
	//�������Ϣ��
	db_ref = DmOpenDatabaseByTypeCreator('data', appFileCreator, dmModeReadWrite);
	//��ȡ�����Ϣ��������
	mb_num = DmNumRecordsInCategory(db_ref, dmAllCategories);
	if (mb_num > 1)
	{
		//�����б�ѡ��ֵ
		list_selection = LstGetSelection(lstP);
		//�����ǰ�б�
		for (i = 0; i < mb_num; i ++)
		{
			MemPtrFree((*mb_list)[i]);
		}
		//��ȡ�ƶ�����
		if (direction == UP) //����
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
		else //����
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
		//�ƶ���¼
		DmMoveRecord(db_ref, record_index, obj_record_index);
		//���»�ȡ�б�
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
				(*mb_list)[i][len-4]=0x1a;//�������
				(*mb_list)[i][len-3]=0;
			}
			else
			{
				(*mb_list)[i] = (Char *)MemPtrNew(len + 1);
				StrCopy((*mb_list)[i], record->file_name);
			}
			MemHandleUnlock(record_handle);
		}
		//�󶨵��б�
		LstSetListChoices(lstP,(*mb_list), mb_num);
		LstDrawList(lstP);
		//�趨��ѡ�����
		LstSetSelection(lstP, list_selection);
	}
	//�ر������Ϣ��
	DmCloseDatabase(db_ref);
}
//--------------------------------------------------------------------------
//�������뷨����״̬
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
			//�򿪳�ʼģʽ��Ϣ��
			db_ref = DmOpenDatabaseByTypeCreator('init', appFileCreator, dmModeReadWrite);
			if (DmGetLastErr()) //���ݿⲻ���ڣ��½�
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
//ɾ�����
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
			//ȡ��ָ��
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
//����ɫ������
static void PaintCurrentColor(stru_Pref *pref)
{
	RectangleType		rectangle;
	RGBColorType		default_rgb_color;
	RGBColorType		backRGBColor;
	
	rectangle.topLeft.x = 67;
	rectangle.topLeft.y = 20;
	rectangle.extent.x = 6;
	rectangle.extent.y = 10;
	//�����ɫ
	WinSetForeColorRGB(&pref->caretColor, &default_rgb_color);
	WinSetBackColorRGB(&default_rgb_color, &backRGBColor);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//�߿���ɫ
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->frameColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//����ǰ��
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->codeForeColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//����ָʾɫ
	if ( pref->javaStatusStyle != 0)
	{
		WinSetForeColorRGB(&pref->chineseStatusColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//���뱳��
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->codeBackColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//Ӣ��ָʾɫ
	if ( pref->javaStatusStyle != 0)
	{
		WinSetForeColorRGB(&pref->englishStatusColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//���ǰ��
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultForeColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//�������
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultBackColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//�����нǱ�Եɫ
	if (pref->javaStatusStyle == 3)
	{
		WinSetForeColorRGB(&pref->chineseEdgeColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//�������ǰ��
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultHighlightForeColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//Ӣ���нǱ�Եɫ
	if (pref->javaStatusStyle == 3)
	{
		WinSetForeColorRGB(&pref->englishEdgeColor, NULL);
		rectangle.topLeft.x = 144;
		WinDrawRectangle(&rectangle, 0);
		WinEraseRectangleFrame(rectangleFrame, &rectangle);
		rectangle.topLeft.x = 67;
	}
	//�����������
	rectangle.topLeft.y += 15;
	WinSetForeColorRGB(&pref->resultHighlightBackColor, NULL);
	WinDrawRectangle(&rectangle, 0);
	WinEraseRectangleFrame(rectangleFrame, &rectangle);
	//�ָ�ǰ��ɫ
	WinSetForeColorRGB(&default_rgb_color, NULL);
	WinSetBackColorRGB(&backRGBColor, NULL);
}
//--------------------------------------------------------------------------
//���ı���
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
//����Ĭ�Ϸ���
static void ResetSign(stru_Pref *pref)
{
	/*Char lp_str[26][16]={"&", "#", "8", "4", "1", "5", "6",\
						 "��", "@", "��", "��", "����", "��", "��",\
						 "����", "?d", "/",      "2", "��", "3",\
						 "����", "9", "+",      "7", "����", "*"};*/
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
		//�ͷ��ڴ�    
		MemHandleUnlock(listHandle);
		MemHandleFree(listHandle);		
	}
	DmReleaseResource(rscHandle);	
}
//--------------------------------------------------------------------------
//�Զ������
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
					case cbEnglishPunc: //Ӣ�ı��
					{
						pref->english_punc = ! pref->english_punc;
						break;
					}
					case cbFullwidth: //ȫ�Ƿ���
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
					case cbOptFullwidth: //Opt�Ƿ����ȫ�Ƿ���
					{
						pref->opt_fullwidth = ! pref->opt_fullwidth;
						break;
					}
					case cbNumFullwidth: //ȫ������
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
	//��������
	PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	//����
	FrmReturnToForm(0);
}
//
static void * GetObjectPtr(FormPtr form, UInt16 objectID)
{
    return FrmGetObjectPtr(form, FrmGetObjectIndex(form, objectID));
}

//--------------------------------------------------------------------------
//�Զ������
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
				
				//��Javaģʽ��ʾ��Ӣ��ʾ
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
						pref->onlyJavaModeShow = true;//��Javaģʽ��ʾ��Ӣ����ʾ
						FrmSetControlValue(form, FrmGetObjectIndex(form, cbOnlyJavaModeShow),pref->onlyJavaModeShow);//��Javaģʽ��ʾ��Ӣ����ʾ
						pref->javaStatusStyle = Style1;	//Java��DTG״̬��ָʾ��ʽ
						lstP = FrmGetObjectPtr(form,FrmGetObjectIndex(form,listJavaStatusStyle));//�б�ָ�Ĭ��
						LstSetSelection(lstP,pref->javaStatusStyle);//�б�ָ�Ĭ��
						CtlSetLabel(triP,LstGetSelectionText(lstP,pref->javaStatusStyle));//�б�ָ�Ĭ��
						pref->javaStatusStyleX = 20;//Java��DTG״̬��ָʾ��ʽ֮�����Ĭ�Ͽ��
						pref->javaStatusStyleY = 20;//Java��DTG״̬��ָʾ��ʽ֮�����Ĭ�ϸ߶�
						
						//����ָ�Ĭ��
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
						
						//ɫ�ʻָ�Ĭ��
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
						UIColorGetTableEntryRGB(UIFormFrame, &pref->caretColor);								//���
						UIColorGetTableEntryRGB(UIDialogFrame, &pref->frameColor);								//�߿�
						UIColorGetTableEntryRGB(UIObjectForeground, &pref->codeForeColor);						//�ؼ�����ɫ
						UIColorGetTableEntryRGB(UIDialogFill, &pref->codeBackColor);							//�ؼ��ֱ���
						UIColorGetTableEntryRGB(UIObjectForeground, &pref->resultForeColor);					//��ѡ����ɫ
						UIColorGetTableEntryRGB(UIObjectFill, &pref->resultBackColor);							//��ѡ�ֱ���
						UIColorGetTableEntryRGB(UIObjectSelectedForeground, &pref->resultHighlightForeColor);	//��ѡ�ָ�����ɫ
						UIColorGetTableEntryRGB(UIObjectSelectedFill, &pref->resultHighlightBackColor);			//��ѡ�ָ�������
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
	//��������
	PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, sizeof(stru_Pref), true);
	//����
	FrmReturnToForm(0);
}
//
//��ʼ��Ĭ������
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
	//�����ͺ�
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
	
	//Ĭ������
	pref->displayFont = boldFont;
	//Ĭ�ϰ����ӳ�
	KeyRates(false, &pref->defaultKeyRate, &periodP, &doubleTapDelayP, &queueAheadP);
	//������Χ
	pref->keyRange[0] = keyA;
	pref->keyRange[1] = keyZ;
	//Ĭ�ϰ���
	pref->Selector[0] = 0x0020; pref->Selector[1] = keyZero; pref->Selector[2] = hsKeySymbol;
	pref->Selector[3] = keyLeftShift; pref->Selector[4] = keyRightShift;
	//Ĭ�ϰ���2
	pref->Selector2[0] = 0; pref->Selector2[1] = 0; pref->Selector2[2] = 0;
	pref->Selector2[3] = 0; pref->Selector2[4] = 0;
	//����ģʽ
	pref->KBMode = KBModeTreo;
	//���ⰴ��
	pref->IMESwitchKey = keyHard1;
	pref->JavaActiveKey = keySpace;
	pref->KBMBSwitchKey = 0;
	pref->MBSwitchKey = 0;
	pref->TempMBSwitchKey = 0;	
	pref->PuncKey = 0;
	pref->PuncType = 0;
	pref->ListKey = hsKeySymbol;
	pref->SyncopateKey = keyPeriod;
	//�������뷨״̬
	pref->shouldShowfloatBar = true;
	pref->DTGSupport = false;
	pref->AutoMBSwich = false;
	pref->LongPressMBSwich = true;
	//������ʽ
	pref->init_mode = initDefaultChinese;
	//���һ�����뷨״̬
	pref->last_mode = imeModeChinese;
	pref->init_mode_record = 0;
	pref->current_field = NULL;
	pref->field_in_table = false;
	//��ʼ����������
	pref->hasShiftMask = false;
	pref->hasOptionMask = false;
	pref->isLongPress = false;
	pref->longPressHandled = false;
	//Ĭ�Ϲ����ɫ
	UIColorGetTableEntryRGB(UIFieldCaret, &pref->defaultCaretColor);
	//��ǰ����
	pref->curWin = NULL;
	//Ĭ�Ͻ���
	pref->onlyJavaModeShow = true;
	UIColorGetTableEntryRGB(UIFormFrame, &pref->caretColor);								//���
	UIColorGetTableEntryRGB(UIDialogFrame, &pref->frameColor);								//�߿�
	UIColorGetTableEntryRGB(UIObjectForeground, &pref->codeForeColor);						//�ؼ�����ɫ
	UIColorGetTableEntryRGB(UIDialogFill, &pref->codeBackColor);							//�ؼ��ֱ���
	UIColorGetTableEntryRGB(UIObjectForeground, &pref->resultForeColor);					//��ѡ����ɫ
	UIColorGetTableEntryRGB(UIObjectFill, &pref->resultBackColor);							//��ѡ�ֱ���
	UIColorGetTableEntryRGB(UIObjectSelectedForeground, &pref->resultHighlightForeColor);	//��ѡ�ָ�����ɫ
	UIColorGetTableEntryRGB(UIObjectSelectedFill, &pref->resultHighlightBackColor);			//��ѡ�ָ�������
	
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
	pref->javaStatusStyle = Style1;	//Java��DTG״̬��ָʾ��ʽ
	pref->javaStatusStyleX = 20;//Java��DTG״̬��ָʾ��ʽ֮�����Ĭ�Ͽ��
	pref->javaStatusStyleY = 20;//Java��DTG״̬��ָʾ��ʽ֮�����Ĭ�ϸ߶�
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
	//��̬����
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
	ResetSign(pref);//�Զ������
}
//
//��⺺����Ϣ�Ƿ����
static Boolean IsDictExist(void)
{
	DmOpenRef db_ref = NULL;	
	FileRef db_file_ref = NULL;
	Boolean exist=false;
	//�򿪺�����Ϣ���ݿ�
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
		while (vol_iterator != vfsIteratorStop)//��ȡ���濨����,ȡ��ָ��
		{
			VFSVolumeEnumerate(&vol_ref, &vol_iterator);
		}	
		if(vol_ref > 0)//���ڴ���û�ҵ����ݿ⣬�����ڿ�����
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
//���ý���
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
	

	pref_size = sizeof(stru_Pref); //pref�ߴ�
	//�Ƿ����Ѵ��ڵ�prefָ��
	if (FtrGet(appFileCreator, ftrPrefNum, &pref_address) == ftrErrNoSuchFeature)
	{
		pref = (stru_Pref *)MemPtrNew(pref_size);
		MemPtrSetOwner(pref, 0); //����Ϊϵͳ����
		MemSet(pref, pref_size, 0x00); //����
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
	//��������������
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
						lstP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstMBList)); //����б�ָ��
						lstPopP = (ListType *)FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, lstInitMode)); //������ʽ�б�ָ��
						//����װ�ص��ڴ��еĴ��濨���
						UnloadMB(unloadAll, true);
						//���������Ϣ���ݿ⣬��ȡ�����������б�
						mb_num = UpdateMBListDB(&mb_list);							

						//װ��pref
						if (! pref_exist)
						{
							if (PrefGetAppPreferences(appFileCreator, appPrefID, pref, &pref_size, true) == noPreferenceFound)
							{
								DefaultPref(pref);
							}
							//��ʼ�������Ϣ
							MemSet(&pref->curMBInfo, sizeof(stru_MBInfo), 0x00);
							//����
							pref->keyDownDetected = false;
							//PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
						}
						pref->activeStatus &= (~optActiveJavaMask);
						PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
						
						if (pref->Enabled)
						{
							MemPtrSetOwner(pref, 0);
						}
						if(IsDA)//DA���к��л����� Ȼ���˳�
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
						if(IsDictExist())//��⺺����Ϣ�Ƿ���ڣ�����������ֵ����������
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
							case btnMBDelete: //ɾ�����
							{
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, btnMBDelete), 0);
								if (mb_num > 0) //��������ɾ��
								{
									//����װ�ص��ڴ��еĴ��濨���
									UnloadMB(unloadAll, true);
									//ɾ�����
									DeleteMB(LstGetSelection(lstP));
									for (i = 0; i < mb_num; i ++)
									{
										MemPtrFree(mb_list[i]);
									}
									if (mb_list != NULL)
									{
										MemPtrFree(mb_list);
									}
									//���������Ϣ���ݿ⣬��ȡ�����������б�
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
							case ptriInitMode: //������ʽ����
							{
								SetInitModeTrigger(LstPopupList(lstPopP), pref);
								break;
							}
							case btnSetBlur: //����ģ����
							{
								if (LstGetSelection(lstP) != noListSelection)
								{
									SetBlurEventHandler(pref, (UInt16)LstGetSelection(lstP));
								}
								break;
							}
							case cbDyncLoad: //��̬�������
							{
								pref->dync_load = ! pref->dync_load;
								break;
							}
							case cbExtractChar: //�Ƿ������Դʶ���
							{
								pref->extractChar = ! pref->extractChar;
								break;
							}	
							case cbAutoSend: //�Ƿ������Զ�����
							{
								pref->autoSend = ! pref->autoSend;
								break;
							}														
							case cbAltChar: //�Ƿ������ַ�ת��
							{
								pref->altChar = ! pref->altChar;
								break;
							}
							case cbSuggestChar: //�Ƿ������������
							{
								pref->suggestChar = ! pref->suggestChar;
								break;
							}		
							case cbFilterGB: //�ַ������Ƿ����ʾGB2312�ַ�
							{
								pref->filterGB = ! pref->filterGB;
								break;
							}
							case cbFilterChar: //��������
							{
								pref->filterChar = ! pref->filterChar;
								break;
							}							
							case btnBackToPref: //�˳�
							{
								MemSet(&event, sizeof(EventType), 0x00);
								event.eType = appStopEvent;
								break;
							}
							case cbEnabledTS: //�Ƿ����ô�Ƶ����
							{
								if (LstGetSelection(lstP) != noListSelection && (mb_num > 0))
								{
									mb_info.frequency_adjust = (FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cbEnabledTS))) !=0; //���ô�Ƶ����
									SetMBInfoByNameType(mb_info.file_name, mb_info.db_type, mb_info.inRAM, &mb_info);
								}
								break;
							}
							case cbDynTips: //��������
							{
								if (LstGetSelection(lstP) != noListSelection && (mb_num > 0))
								{
									mb_info.gradually_search = (FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cbDynTips)))!=0;
									SetMBInfoByNameType(mb_info.file_name, mb_info.db_type, mb_info.inRAM, &mb_info);
								}
								break;
							}
							case cbMBEnable: //���á�ͣ�����
							{
								if (LstGetSelection(lstP) != noListSelection)
								{
									mb_info.enabled = (Boolean)FrmGetControlValue(frmP, FrmGetObjectIndex(frmP, cbMBEnable));
									SetMBInfoFormMBList(&mb_info, (UInt16)LstGetSelection(lstP));
								}
								break;
							}
							case btnMBUp: //����
							case btnMBDown: //����
							{
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, event.data.ctlSelect.controlID), 0);
								if ((LstGetSelection(lstP) > 0 && event.data.ctlSelect.controlID==btnMBUp)\
									|| (LstGetSelection(lstP) < LstGetNumberOfItems(lstP) - 1 && event.data.ctlSelect.controlID==btnMBDown))
								{
									MoveMBRecordInMBListDB((UInt16)LstGetSelection(lstP), lstP, &mb_list, event.data.ctlSelect.controlID - btnMBUp);
								}
								break;
							}
							case pbtnEnable: //��ͣ���뷨
							{														
								if (pref->Enabled == false) //����
								{
									//��ȡ�����Ϣ���һ�����õ����
									for (i = 0; i < mb_num; i ++)
									{
										if (MBEnabled(i))
										{
											pref->Enabled = true;
											break;
										}
									}
								}
								else //ͣ��
								{
									pref->Enabled = false;
								}
								FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, pbtnEnable), pref->Enabled);			
								break;
							}
						}
						break;
					}
					case lstSelectEvent: //��ʾ���ϸ��
					{
						if (event.data.lstSelect.selection >= 0)
						{
							//��ȡ��Ϣ
							GetMBInfoFormMBList(&mb_info, event.data.lstSelect.selection, true, true);
							//�Ƿ�����
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbMBEnable), (Int16)mb_info.enabled);
							//�Ƿ����ģ����
							if (mb_info.smart_offset != 0)
							{
								FrmShowObject(frmP, FrmGetObjectIndex(frmP, btnSetBlur));
							}
							else
							{
								FrmHideObject(frmP, FrmGetObjectIndex(frmP, btnSetBlur));
							}
							//��������
							FrmSetControlValue(frmP, FrmGetObjectIndex(frmP, cbDynTips), mb_info.gradually_search);
							//��Ƶ����
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
	
	//���ڴ�ж�����б����ص����
	UnloadMB(unloadAll, true);
	//�ع�prefָ����Ϣ�����ͷ�ԭprefָ���е��Զ������ͼ�ֵת������
	if (pref->curMBInfo.syncopate_offset > 0 && pref->curMBInfo.key_syncopate != NULL)
	{
		MemPtrFree(pref->curMBInfo.key_syncopate);
	}
	if (pref->curMBInfo.translate_offset > 0 && pref->curMBInfo.key_translate != NULL)
	{
		MemPtrFree(pref->curMBInfo.key_translate);
	}
	//���pref�������Ϣ������
	MemSet(&pref->curMBInfo, sizeof(stru_MBInfo), 0x00);
	SysCurAppDatabase(&cardNo, &dbID);
	if (! pref->Enabled) //���뷨δ�������ͷ�ȫ��pref���ݣ�ȡ��������Ϣ��ע��
	{
		FtrUnregister(appFileCreator, ftrPrefNum);
		SetKeyRates(true, pref);
		SetCaretColor(true, pref);
		SysNotifyUnregister(cardNo, dbID, sysNotifyInsPtEnableEvent, sysNotifyNormalPriority);
		SysNotifyUnregister(cardNo, dbID, sysNotifyEventDequeuedEvent, pref->NotifyPriority);
		//��������
		PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
		MemPtrFree(pref);
	}
	else //���뷨������������pref�������Ϣ����
	{
		//��ȡ�����Ϣ���һ�����õ����
		for (i = 0; i < mb_num; i ++)
		{
			if (MBEnabled(i))
			{
				GetMBInfoFormMBList(&pref->curMBInfo, i, true, pref->dync_load);
				GetMBDetailInfo(&pref->curMBInfo, true, pref->dync_load);
				pref->last_mode = imeModeChinese;
				//��������
				PrefSetAppPreferences(appFileCreator, appPrefID, appPrefVersionNum, pref, pref_size, true);
				
				MemPtrSetOwner(pref, 0);
				SysNotifyRegister(cardNo, dbID, sysNotifyEventDequeuedEvent, NULL, pref->NotifyPriority, pref);
				SysNotifyRegister(cardNo, dbID, sysNotifyInsPtEnableEvent, NULL, sysNotifyNormalPriority, pref);
				break;
			}
		}
	}
	
	//�ͷ��ڴ�
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
	//����Launcher���ǿ���̨
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