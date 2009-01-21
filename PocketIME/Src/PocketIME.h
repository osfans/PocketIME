/*
 * PocketIME.h
 *
 * header file for PocketIME
 *
 * This wizard-generated code is based on code adapted from the
 * stationery files distributed as part of the Palm OS SDK 4.0.
 *
 * Copyright (c) 1999-2000 Palm, Inc. or its subsidiaries.
 * All rights reserved.
 */
 
#ifndef POCKETIME_H_
#define POCKETIME_H_

/*********************************************************************
 * Internal Structures
 *********************************************************************/

//����б���Ϣ
typedef struct
{
	Boolean			MBEnabled;			//����Ƿ�����
	UInt32			MBDbType;			//����type
	Boolean			inRAM;				//������ڴ�
	Char			file_name[32];		//����ļ���
}stru_MBList;

//ģ������Ϣ
typedef struct
{
	Char			key1[5];			//��Ӧ��ֵ1
	Char			key2[5];			//��Ӧ��ֵ2
	Boolean			actived;			//�Ƿ񱻼���
}stru_BlurInfo;

//�����Ϣ
typedef struct
{
	Boolean			enabled;			//�������
	Char			file_name[32];		//����ļ���
	UInt32			db_type;			//����ļ�����
	Boolean			inRAM;				//������ڴ�
	Char			name[9];			//�������
	UInt8			type;				//�������
	UInt32			index_offset;		//����ƫ����
	UInt8			key_length;			//�볤
	Char			used_char[30];		//��ֵ��Χ
	Char			wild_char;			//���ܼ�
	UInt32			syncopate_offset;	//ȫ��Ԫƫ����
	UInt32			syncopate_size;		//ȫ��Ԫ�ߴ�
	UInt32			translate_offset;	//��ֵת��ƫ����
	UInt32			translate_size;		//��ֵת���ߴ�
	UInt32			smart_offset;		//ģ����ƫ����
	UInt32			smart_size;			//ģ����ߴ�
	Boolean			gradually_search;	//��������
	Boolean			frequency_adjust;	//��Ƶ����
	Char			*key_syncopate;		//�Զ�������Ϣ
	Char			*key_translate;		//��ֵת����Ϣ
	stru_BlurInfo	blur_head[11];		//ģ����-ǰ
	stru_BlurInfo	blur_tail[11];		//ģ����-��
}stru_MBInfo;

#define FontGroup					2

//���뷨������Ϣ
typedef struct
{
	Boolean				Enabled;			//�Ƿ��������뷨
	Boolean				Actived;			//���뷨�Ƿ��Ծ
	Int8				NotifyPriority;		//���뷨�����ȼ�
	
	RGBColorType		defaultCaretColor;	//Ĭ�Ϲ����ɫ
	UInt16				defaultKeyRate;		//ϵͳĬ�ϰ�����ʱ
	
	UInt8				isTreo;				//�Ƿ�Treoϵ��
	FontID				displayFont;		//��ʾ���ֵ�����
	UInt8				init_mode;			//Ĭ��������ʽ
	UInt8				last_mode;			//���һ�����뷨״̬
	UInt16				init_mode_record;	//��ǰ���뷨״̬�ļ�¼��
	FieldType			*current_field;		//��ǰ������ı���
	Boolean				field_in_table;		//��ǰ������ı������ڱ��
	
	UInt8				KBMode;				//��������ģʽ
	WChar				keyRange[2];		//��Ч�İ�����Χ
	WChar				Selector[5];		//ѡ�ְ���
	WChar				Selector2[5];		//�ڶ���ѡ�ְ���
	WChar				IMESwitchKey;		//���뷨״̬�л���
	WChar				KBMBSwitchKey;		//���ü���ģʽ-����л���
	WChar				MBSwitchKey;		//����л���
	WChar				TempMBSwitchKey;	//�����ʱ�л���, ����һ���ֺ�ָ�ԭ���
	WChar				PuncKey;			//�����̼�
	UInt8				PuncType;			//��������
	WChar				ListKey;			//�����б��	
	WChar				SyncopateKey;		//������
	WChar				MenuKey;			//�˵���
	WChar				JavaActiveKey;		//Java��DTG������	
	Boolean				shouldShowfloatBar;	//��ʾ���������
	Boolean				DTGSupport;			//֧��Java
	Boolean				AutoMBSwich;		//֧���Զ��л����
	Boolean				LongPressMBSwich;	//�����л����,Opt+�л�
	
	UInt8				activeStatus;		//����״̬��־
	Boolean				hasShiftMask;		//������Shift��־
	Boolean				hasOptionMask;		//������Option��־
	Boolean				isLongPress;		//�ǳ���
	Boolean				longPressHandled;	//�������򰴼����¼��Ѵ���
	WinHandle			curWin;				//��ǰ����
	
	RGBColorType		caretColor;					//�����ɫ
	RGBColorType		frameColor;					//�߿�ɫ
	RGBColorType		codeForeColor;				//���벿��ǰ��ɫ
	RGBColorType		codeBackColor;				//���벿�ֱ���ɫ
	RGBColorType		resultForeColor;			//�������ǰ��ɫ
	RGBColorType		resultBackColor;			//������ֱ���ɫ
	RGBColorType		resultHighlightForeColor;	//������ָ�������ɫ
	RGBColorType		resultHighlightBackColor;	//������ָ�������ɫ
	RGBColorType		chineseStatusColor;			//Java��DTG״̬�£�ָʾɫ������
	RGBColorType		englishStatusColor;			//Java��DTG״̬�£�ָʾɫ��Ӣ��
	
	RGBColorType		chineseEdgeColor;			//Java��DTG״̬�£������нǱ�Եɫ
	RGBColorType		englishEdgeColor;			//Java��DTG״̬�£�Ӣ���нǱ�Եɫ
	
	UInt8				javaStatusStyle;			//Java��DTG״̬��ָʾ��ʽ
	UInt8				javaStatusStyleX;			//Java��DTG״̬��ָʾ��ʽ����Ŀ��
	UInt8				javaStatusStyleY;			//Java��DTG״̬��ָʾ��ʽ����ĸ߶�
	
	Boolean				filterGB;			//�Ƿ����ʾGB2312�ַ���
	Boolean				filterChar;			//�Ƿ����ʾ����
	Boolean				dync_load;			//��̬�������
	Boolean				english_punc;		//���Ӣ�ı��
	Boolean				fullwidth;			//ȫ�Ƿ���
	Boolean 			opt_fullwidth;		//opt+�����Ƿ�Ҳ���ȫ��
	Boolean				num_fullwidth;		//�����Ƿ�Ҫ���ȫ��
	Boolean				choice_button;		//��ʾѡ�ְ�ť
	Boolean				menu_button;		//��ʾ�˵���ť
	Boolean				keyDownDetected;	//��⵽�������¼�
	Boolean				suggestChar;		//�Ƿ��������
	Boolean				altChar;			//�Ƿ������ַ�ת��
	Boolean				extractChar;		//�Ƿ������Դʶ���
	Boolean 			autoSend;			//�Ƿ��Զ�����
	
	Boolean				onlyJavaModeShow;	//��Javaģʽ��ʾ��Ӣ��ָʾ״̬
	Boolean				showGsi;			//��ʾGSIָʾ��
	
	stru_MBInfo			curMBInfo;			//��ǰ������Ϣ
	
	Char				CustomLP[26][16];				//�Զ��峤������
	Char				CustomLPPeriod[16];			//�Զ��峤������
	Char				CustomLPShiftPeriod[16];	//�Զ�����Ϸ���
	Char				CustomLPOptBackspace[16];	//�Զ�����Ϸ���
	Char				CustomLPShiftBackspace[16];	//�Զ�����Ϸ���
}stru_Pref;

//��Ӣ��״̬��Ϣ���¼��Ԫ�ṹ
typedef struct
{
	UInt16			form_id;
	UInt16			object_count;
	UInt16			field_id;
	UInt8			mode;
}stru_InitInfo;
#define stru_InitInfo_length	8

//����¼�е����ݵ�Ԫ
typedef struct
{
	MemHandle		content;		//����
	void			*next;			//��һ���ڵ�
}stru_MBContent;
#define stru_MBContent_length	8

//����¼�е�������Ԫ
typedef struct
{
	UInt16			index;			//����ֵ
	UInt16			offset;			//ƫ����
	void			*next;			//��һ���ڵ�
}stru_MBIndex;
#define stru_MBIndex_length		8

//����ʽ������
typedef struct
{
	Char			result[50];		//�������
	UInt16			length;			//�������
	UInt16			record_index;	//������ڵļ�¼��
	Char			index[5];		//�������������ֵ
	UInt16			offset;			//������ڵļ�¼��ƫ����
}stru_CreateWordResult;
//�������ߴ�

//�����������
typedef struct
{
	Char			*result;		//�������
	Char			*key;			//������� �볤����4�Ĳ��ʻ����뷨
	UInt16			length;			//�������
	UInt16			record_index;	//������ڵļ�¼��
	Char			index[5];		//�������������ֵ	
	Boolean			is_static;		//�Ƿ�̶�����
	UInt16			offset;			//������ڵļ�¼��ƫ����
	void			*next;			//��һ�����
	void			*prev;			//��һ�����
}stru_Result;
//�������ߴ�
#define	stru_Result_length		26

//���������е�ƫ��������
typedef struct
{
	UInt16			key;				//��ֵ
	UInt16			offset;				//ƫ����
	void			*next;				//��һ��
}stru_ContentOffset;
//��������ߴ�
#define	stru_ContentOffset_length	8

//��������
typedef struct
{
	Char				index[5];			//�ü�¼������
	UInt16				record_index;		//������ڵļ�¼��
	UInt16				last_word_length;	//���һ������ĳ���
	Boolean				more_result_exist;	//���н�����Լ���
	stru_ContentOffset	offset_head;		//��������ƫ��������ı�ͷ
	stru_ContentOffset	offset_tail;		//��������ƫ��������ı�β
	void				*next;				//��һ���������
	void				*prev;				//��һ���������
}stru_MBRecord;
//��������ߴ�
#define	stru_MBRecord_length		36

//�ؼ��ֻ��浥Ԫ
typedef struct
{
	Char			content[100];		//�ؼ�������
	UInt16			length;				//�ؼ��ֳ���
}stru_KeyBufUnit;
#define stru_KeyBufUnit_length		102

//�ؼ��ֻ���
typedef struct
{
	stru_KeyBufUnit	key[10];			//�ؼ��ֻ���
	UInt16			key_index;			//��ǰ�ؼ�������
}stru_KeyBuf;

//����ʱ��Ϣ
typedef struct
{
	Char					cache[512];			//��ʱ����

	Boolean					no_prev;			//�Ƿ����Ϸ�
	Boolean					no_next;			//�Ƿ����·�
	
	Boolean					in_create_word_mode;//�����ģʽ
	UInt8					created_word_count;	//����ʻ������
	UInt16					created_key;		//���������ʵ�
	stru_CreateWordResult	created_word[10];	//����ʻ���
	
	stru_KeyBuf				key_buf;			//�ؼ��ֻ���
	stru_KeyBufUnit			blur_key[5];		//ģ�����ؼ���
	Boolean					new_key;			//�½��ؼ��ֱ��
	Boolean					english_mode;		//�����Ӣ��ģʽ
	
	UInt16					current_word_length;//��ǰ�������
	stru_MBRecord			*mb_record_head;	//��������ļ�¼��ѭ������ı�ͷ
	stru_MBRecord			*mb_record;			//��������ļ�¼��ѭ������ǰ�ڵ�
	stru_Result				result_head;		//��������ͷ
	stru_Result				result_tail;		//��������β
	stru_Result				*result;			//����������һ����Ч�ڵ�
	
	DmOpenRef				db_ref;				//������ݿ�ָ��
	FileRef					db_file_ref;		//������ݿ�ָ�루���ϣ�
	
	WinHandle				draw_buf;			//��ͼ����
	Boolean					in_menu;			//�����˵��Ƿ񼤻�
	
	UInt8					result_status[100];	//ÿҳ�Ľ����ʾ���
	UInt8					page_count;			//��ǰ��ҳ��
	UInt8					cursor;				//��ǰѡ�����
	
	FormType				*imeFormP;
	MenuBarType				*imeMenuP;
	RectangleType			imeFormRectangle;
	stru_Pref				*settingP;
	WChar					initKey;
	Char					*bufP;
	
	RectangleType			resultRect[5];		//ѡ����
	UInt8					curCharWidth;
	UInt8					curCharHeight;
	RectangleType			oneResultRect;
}stru_Globe;
//ȫ�ֻ���ߴ�
#define	stru_Globe_length		2894

//���ݼ�¼����
typedef struct
{
	MemHandle	recH;
	Char		*recP;
}stru_recData;
/*********************************************************************
 * Global variables
 *********************************************************************/


/*********************************************************************
 * Internal Constants
 *********************************************************************/

#define appFileCreator				'pIME'
#define appName						"PocketIME"
#define appVersionNum				0x01
#define appPrefID					0x00
#define appPrefVersionNum			0x05

#define sysAppLaunchCmdDALaunch			60000
#define sysAppLaunchCmdDALaunchJava			60001

#define PIME_CARD_PATH				"/PALM/Programs/PocketIME/"
#define PIME_CARD_PATH_DICT			"/PALM/Programs/PocketIME_Dict.pdb"

#define	ByteSwap16(n) ( ((((unsigned int) n) << 8) & 0xFF00) | ((((unsigned int) n) >> 8) & 0x00FF) )

#define ftrPrefNum					0x01				//����ʱ������Ϣ��ָ��
#define ftrXplore					0x02				//Ȩ�Ǽ��̰�����¼

#define	frmIMEForm					0x1B58				//���뷨����ID
#define btnChr1						0x1B59
#define btnChr2						0x1B5A
#define btnChr3						0x1B5B
#define btnChr4						0x1B5C
#define btnChr5						0x1B5D
#define btnChrUP					0x1B5E
#define btnChrDOWN					0x1B5F
#define btnChrMENU					0x1B60
#define btnChrQ						0x1B61

#define frmAlt						0x1B62				//������Ϣ����ID
#define lstAlt						0x1B63

#define	fixModeNormal				0x00				//��Ƶ����ģʽ-��������
#define fixModeTop					0x01				//��Ƶ����ģʽ-ǿ���ƶ�����һ��λ��

#define	UP							0x00				//����б�Ԫ�����ƶ�
#define DOWN						0x01				//����б�Ԫ�����ƶ�

#define	LOAD						0x00				//�������
#define SAVE						0x01				//ж�����

#define GetTranslatedKey			0x00				//��ֵת��-��ȡת����ļ�ֵ
#define GetKeyToShow				0x01				//��ֵת��-��ȡ��ʾ�õļ�ֵ

#define	unloadAll					0x00				//ж��ȫ�������ڴ�Ĵ��濨���
#define unloadWithoutDefault		0x01				//ж�س�Ĭ��������ⱻ�����ڴ�Ĵ��濨���

#define initDefaultChinese			0x00				//������ʽ-Ĭ������
#define initDefaultEnglish			0x01				//������ʽ-Ĭ��Ӣ��
#define initKeepLast				0x02				//������ʽ-���״̬
#define initRememberFav				0x03				//������ʽ-��ס״̬

#define imeModeChinese				0x00				//���뷨״̬-����
#define imeModeEnglish				0x01				//���뷨״̬-Ӣ��

#define KBModeTreo					0x00				//��������ģʽ-Treo
#define KBModeExt					0x01				//��������ģʽ-���ü���
#define KBModeXplore				0x02				//��������ģʽ-Ȩ�Ǽ���
#define KBModeExtFull				0x03				//����101������

#define pchrZero					0x1920				//Ȩ�Ǽ��̼�ֵ
#define pchrOne						0x1921
#define pchrTwo						0x1922
#define pchrThree					0x1923
#define pchrFour					0x1924
#define pchrFive					0x1925
#define pchrSix						0x1926
#define pchrSeven					0x1927
#define pchrEight					0x1928
#define pchrNine					0x1929
#define pchrAsterisk				0x192A
#define pchrNumberSign				0x192B
#define pchrBackspace				0x192C


#define isTreo600					0x01				//Treo�豸��ʶ-Treo 600
#define isTreo650					0x02				//Treo�豸��ʶ-Treo 650/680��Centro

#define tempDisabledMask			0x01				//���뷨״̬-Ӣ��״̬
#define inJavaMask					0x02				//���뷨״̬-Java��DTGģʽ
#define optActiveJavaMask			0x04				//���뷨״̬-Java��DTG����

#define tempMBSwitchMask			0x08				//��ʱ�л����״̬ - ��ʱ���״̬

#define optionTempState				0x01				//Grfָʾ��״̬
#define optionLockState				0x02
#define shiftTempState				0x03
#define shiftLockState				0x04

#define cursorLeft					0x00				//��������ƶ�-����
#define cursorRight					0x01				//��������ƶ�-����

#define slot1						0x01				//�����ҳ���λ�ñ�ʶ
#define slot2						0x02
#define slot3						0x04
#define slot4						0x08
#define slot5						0x10

#define SelectBySelector			0x00				//ѡ��ģʽ-����ѡ�ּ�
#define SelectByEnterKey			0x10				//ѡ��ģʽ-�س���

#define pimeExit					0x00				//������˳���ʶ-�����˳�
#define pimeCreateWord				0x01				//������˳���ʶ-�˳��󼤻��ֶ���ʽ���
#define pimeReActive				0x02				//������˳���ʶ-�˳������ؽ�������¼���
//#define pimeTempMB					0x03				//��ʱ���

#define NativeKeyDownEvent			0x0400				//�¼�������Ϣ�еļ����¼�ֵ
#define NativeFldEnterEvent			0x0F00				//�¼�������Ϣ�е��ı����¼�ֵ
//#define EventTypeSize				0x18				//�¼��ṹ�ĳߴ�

#define Style1						0x00				//Java��DTG״̬��ָʾ��ʽ
#define Style2						0x01				//Java��DTG״̬��ָʾ��ʽ
#define Style3						0x02				//Java��DTG״̬��ָʾ��ʽ
#define Style4						0x03				//Java��DTG״̬��ָʾ��ʽ

#define FORM_UPDATE_FRAMEONLY		0x0001

#define SUGGEST_LIST_HEIGHT			10					//�����б�߶�

#endif /* POCKETIME_H_ */
