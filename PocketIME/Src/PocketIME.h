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

//码表列表信息
typedef struct
{
	Boolean			MBEnabled;			//码表是否启用
	UInt32			MBDbType;			//码表的type
	Boolean			inRAM;				//码表在内存
	Char			file_name[32];		//码表文件名
}stru_MBList;

//模糊音信息
typedef struct
{
	Char			key1[5];			//对应键值1
	Char			key2[5];			//对应键值2
	Boolean			actived;			//是否被激活
}stru_BlurInfo;

//码表信息
typedef struct
{
	Boolean			enabled;			//码表启动
	Char			file_name[32];		//码表文件名
	UInt32			db_type;			//码表文件类型
	Boolean			inRAM;				//码表在内存
	Char			name[9];			//码表名称
	UInt8			type;				//码表类型
	UInt32			index_offset;		//索引偏移量
	UInt8			key_length;			//码长
	Char			used_char[30];		//键值范围
	Char			wild_char;			//万能键
	UInt32			syncopate_offset;	//全码元偏移量
	UInt32			syncopate_size;		//全码元尺寸
	UInt32			translate_offset;	//码值转换偏移量
	UInt32			translate_size;		//码值转换尺寸
	UInt32			smart_offset;		//模糊码偏移量
	UInt32			smart_size;			//模糊码尺寸
	Boolean			gradually_search;	//渐进查找
	Boolean			frequency_adjust;	//词频调整
	Char			*key_syncopate;		//自动切音信息
	Char			*key_translate;		//码值转换信息
	stru_BlurInfo	blur_head[11];		//模糊音-前
	stru_BlurInfo	blur_tail[11];		//模糊音-后
}stru_MBInfo;

#define FontGroup					2

//输入法配置信息
typedef struct
{
	Boolean				Enabled;			//是否启动输入法
	Boolean				Actived;			//输入法是否活跃
	Int8				NotifyPriority;		//输入法的优先级
	
	RGBColorType		defaultCaretColor;	//默认光标颜色
	UInt16				defaultKeyRate;		//系统默认按键延时
	
	UInt8				isTreo;				//是否Treo系列
	FontID				displayFont;		//显示文字的字体
	UInt8				init_mode;			//默认启动方式
	UInt8				last_mode;			//最后一次输入法状态
	UInt16				init_mode_record;	//当前输入法状态的记录号
	FieldType			*current_field;		//当前激活的文本框
	Boolean				field_in_table;		//当前激活的文本框属于表格
	
	UInt8				KBMode;				//键盘驱动模式
	WChar				keyRange[2];		//有效的按键范围
	WChar				Selector[5];		//选字按键
	WChar				Selector2[5];		//第二组选字按键
	WChar				IMESwitchKey;		//输入法状态切换键
	WChar				KBMBSwitchKey;		//外置键盘模式-码表切换键
	WChar				MBSwitchKey;		//码表切换键
	WChar				TempMBSwitchKey;	//码表临时切换键, 打完一个字后恢复原码表
	WChar				PuncKey;			//符号盘键
	UInt8				PuncType;			//符号类型
	WChar				ListKey;			//联想列表键	
	WChar				SyncopateKey;		//切音键
	WChar				MenuKey;			//菜单键
	WChar				JavaActiveKey;		//Java、DTG启动键	
	Boolean				shouldShowfloatBar;	//显示浮动输入框
	Boolean				DTGSupport;			//支持Java
	Boolean				AutoMBSwich;		//支持自动切换码表
	Boolean				LongPressMBSwich;	//长按切换码表,Opt+切换
	
	UInt8				activeStatus;		//激活状态标志
	Boolean				hasShiftMask;		//按键有Shift标志
	Boolean				hasOptionMask;		//按键有Option标志
	Boolean				isLongPress;		//是长按
	Boolean				longPressHandled;	//长按（或按键）事件已处理
	WinHandle			curWin;				//当前窗口
	
	RGBColorType		caretColor;					//光标颜色
	RGBColorType		frameColor;					//边框色
	RGBColorType		codeForeColor;				//编码部分前景色
	RGBColorType		codeBackColor;				//编码部分背景色
	RGBColorType		resultForeColor;			//结果部分前景色
	RGBColorType		resultBackColor;			//结果部分背景色
	RGBColorType		resultHighlightForeColor;	//结果部分高亮字体色
	RGBColorType		resultHighlightBackColor;	//结果部分高亮背景色
	RGBColorType		chineseStatusColor;			//Java、DTG状态下，指示色，中文
	RGBColorType		englishStatusColor;			//Java、DTG状态下，指示色，英文
	
	RGBColorType		chineseEdgeColor;			//Java、DTG状态下，中文切角边缘色
	RGBColorType		englishEdgeColor;			//Java、DTG状态下，英文切角边缘色
	
	UInt8				javaStatusStyle;			//Java、DTG状态的指示样式
	UInt8				javaStatusStyleX;			//Java、DTG状态的指示样式矩阵的宽度
	UInt8				javaStatusStyleY;			//Java、DTG状态的指示样式矩阵的高度
	
	Boolean				filterGB;			//是否仅显示GB2312字符集
	Boolean				filterChar;			//是否仅显示单字
	Boolean				dync_load;			//动态加载码表
	Boolean				english_punc;		//半角英文标点
	Boolean				fullwidth;			//全角符号
	Boolean 			opt_fullwidth;		//opt+键盘是否也输出全角
	Boolean				num_fullwidth;		//数字是否要输出全角
	Boolean				choice_button;		//显示选字按钮
	Boolean				menu_button;		//显示菜单按钮
	Boolean				keyDownDetected;	//检测到过按下事件
	Boolean				suggestChar;		//是否词语联想
	Boolean				altChar;			//是否允许字符转换
	Boolean				extractChar;		//是否允许以词定字
	Boolean 			autoSend;			//是否自动上屏
	
	Boolean				onlyJavaModeShow;	//仅Java模式显示中英文指示状态
	Boolean				showGsi;			//显示GSI指示器
	
	stru_MBInfo			curMBInfo;			//当前码表的信息
	
	Char				CustomLP[26][16];				//自定义长按符号
	Char				CustomLPPeriod[16];			//自定义长按符号
	Char				CustomLPShiftPeriod[16];	//自定义组合符号
	Char				CustomLPOptBackspace[16];	//自定义组合符号
	Char				CustomLPShiftBackspace[16];	//自定义组合符号
}stru_Pref;

//中英文状态信息库记录单元结构
typedef struct
{
	UInt16			form_id;
	UInt16			object_count;
	UInt16			field_id;
	UInt8			mode;
}stru_InitInfo;
#define stru_InitInfo_length	8

//码表记录中的内容单元
typedef struct
{
	MemHandle		content;		//内容
	void			*next;			//下一个节点
}stru_MBContent;
#define stru_MBContent_length	8

//码表记录中的索引单元
typedef struct
{
	UInt16			index;			//索引值
	UInt16			offset;			//偏移量
	void			*next;			//下一个节点
}stru_MBIndex;
#define stru_MBIndex_length		8

//自造词结果缓存
typedef struct
{
	Char			result[50];		//检索结果
	UInt16			length;			//结果长度
	UInt16			record_index;	//结果所在的记录号
	Char			index[5];		//结果所属的索引值
	UInt16			offset;			//结果所在的记录的偏移量
}stru_CreateWordResult;
//结果缓存尺寸

//检索结果缓存
typedef struct
{
	Char			*result;		//检索结果
	Char			*key;			//结果编码 码长大于4的仓颉笔画输入法
	UInt16			length;			//结果长度
	UInt16			record_index;	//结果所在的记录号
	Char			index[5];		//结果所属的索引值	
	Boolean			is_static;		//是否固顶词组
	UInt16			offset;			//结果所在的记录的偏移量
	void			*next;			//下一个结果
	void			*prev;			//上一个结果
}stru_Result;
//结果缓存尺寸
#define	stru_Result_length		26

//索引缓存中的偏移量链表
typedef struct
{
	UInt16			key;				//键值
	UInt16			offset;				//偏移量
	void			*next;				//下一个
}stru_ContentOffset;
//索引缓存尺寸
#define	stru_ContentOffset_length	8

//索引缓存
typedef struct
{
	Char				index[5];			//该记录的索引
	UInt16				record_index;		//结果所在的记录号
	UInt16				last_word_length;	//最后一个结果的长度
	Boolean				more_result_exist;	//还有结果可以检索
	stru_ContentOffset	offset_head;		//索引缓存偏移量链表的表头
	stru_ContentOffset	offset_tail;		//索引缓存偏移量链表的表尾
	void				*next;				//下一个结果索引
	void				*prev;				//上一个结果索引
}stru_MBRecord;
//索引缓存尺寸
#define	stru_MBRecord_length		36

//关键字缓存单元
typedef struct
{
	Char			content[100];		//关键字内容
	UInt16			length;				//关键字长度
}stru_KeyBufUnit;
#define stru_KeyBufUnit_length		102

//关键字缓存
typedef struct
{
	stru_KeyBufUnit	key[10];			//关键字缓存
	UInt16			key_index;			//当前关键字索引
}stru_KeyBuf;

//运行时信息
typedef struct
{
	Char					cache[512];			//临时缓存

	Boolean					no_prev;			//是否还能上翻
	Boolean					no_next;			//是否还能下翻
	
	Boolean					in_create_word_mode;//自造词模式
	UInt8					created_word_count;	//自造词缓存计数
	UInt16					created_key;		//已完成自造词的
	stru_CreateWordResult	created_word[10];	//自造词缓存
	
	stru_KeyBuf				key_buf;			//关键字缓存
	stru_KeyBufUnit			blur_key[5];		//模糊音关键字
	Boolean					new_key;			//新建关键字标记
	Boolean					english_mode;		//输入框英文模式
	
	UInt16					current_word_length;//当前结果长度
	stru_MBRecord			*mb_record_head;	//包含结果的记录的循环链表的表头
	stru_MBRecord			*mb_record;			//包含结果的记录的循环链表当前节点
	stru_Result				result_head;		//结果链表表头
	stru_Result				result_tail;		//结果链表表尾
	stru_Result				*result;			//结果链表最后一个有效节点
	
	DmOpenRef				db_ref;				//码表数据库指针
	FileRef					db_file_ref;		//码表数据库指针（卡上）
	
	WinHandle				draw_buf;			//绘图缓存
	Boolean					in_menu;			//输入框菜单是否激活
	
	UInt8					result_status[100];	//每页的结果显示情况
	UInt8					page_count;			//当前的页数
	UInt8					cursor;				//当前选定结果
	
	FormType				*imeFormP;
	MenuBarType				*imeMenuP;
	RectangleType			imeFormRectangle;
	stru_Pref				*settingP;
	WChar					initKey;
	Char					*bufP;
	
	RectangleType			resultRect[5];		//选字区
	UInt8					curCharWidth;
	UInt8					curCharHeight;
	RectangleType			oneResultRect;
}stru_Globe;
//全局缓存尺寸
#define	stru_Globe_length		2894

//数据记录资料
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

#define ftrPrefNum					0x01				//运行时配置信息的指针
#define ftrXplore					0x02				//权智键盘按键记录

#define	frmIMEForm					0x1B58				//输入法窗口ID
#define btnChr1						0x1B59
#define btnChr2						0x1B5A
#define btnChr3						0x1B5B
#define btnChr4						0x1B5C
#define btnChr5						0x1B5D
#define btnChrUP					0x1B5E
#define btnChrDOWN					0x1B5F
#define btnChrMENU					0x1B60
#define btnChrQ						0x1B61

#define frmAlt						0x1B62				//汉字信息窗口ID
#define lstAlt						0x1B63

#define	fixModeNormal				0x00				//词频调整模式-正常调整
#define fixModeTop					0x01				//词频调整模式-强制移动到第一个位置

#define	UP							0x00				//码表列表单元向上移动
#define DOWN						0x01				//码表列表单元向下移动

#define	LOAD						0x00				//载入码表
#define SAVE						0x01				//卸下码表

#define GetTranslatedKey			0x00				//键值转换-获取转换后的键值
#define GetKeyToShow				0x01				//键值转换-获取显示用的键值

#define	unloadAll					0x00				//卸载全部载入内存的储存卡码表
#define unloadWithoutDefault		0x01				//卸载除默认码表以外被载入内存的储存卡码表

#define initDefaultChinese			0x00				//启动方式-默认中文
#define initDefaultEnglish			0x01				//启动方式-默认英文
#define initKeepLast				0x02				//启动方式-最后状态
#define initRememberFav				0x03				//启动方式-记住状态

#define imeModeChinese				0x00				//输入法状态-中文
#define imeModeEnglish				0x01				//输入法状态-英文

#define KBModeTreo					0x00				//键盘驱动模式-Treo
#define KBModeExt					0x01				//键盘驱动模式-外置键盘
#define KBModeXplore				0x02				//键盘驱动模式-权智键盘
#define KBModeExtFull				0x03				//外置101键键盘

#define pchrZero					0x1920				//权智键盘键值
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


#define isTreo600					0x01				//Treo设备标识-Treo 600
#define isTreo650					0x02				//Treo设备标识-Treo 650/680、Centro

#define tempDisabledMask			0x01				//输入法状态-英文状态
#define inJavaMask					0x02				//输入法状态-Java、DTG模式
#define optActiveJavaMask			0x04				//输入法状态-Java、DTG激活

#define tempMBSwitchMask			0x08				//临时切换码表状态 - 临时码表状态

#define optionTempState				0x01				//Grf指示器状态
#define optionLockState				0x02
#define shiftTempState				0x03
#define shiftLockState				0x04

#define cursorLeft					0x00				//输入框光标移动-左移
#define cursorRight					0x01				//输入框光标移动-右移

#define slot1						0x01				//输入框当页结果位置标识
#define slot2						0x02
#define slot3						0x04
#define slot4						0x08
#define slot5						0x10

#define SelectBySelector			0x00				//选字模式-光标或选字键
#define SelectByEnterKey			0x10				//选字模式-回车键

#define pimeExit					0x00				//输入框退出标识-正常退出
#define pimeCreateWord				0x01				//输入框退出标识-退出后激活手动造词界面
#define pimeReActive				0x02				//输入框退出标识-退出并返回结果后重新激活
//#define pimeTempMB					0x03				//临时码表

#define NativeKeyDownEvent			0x0400				//事件出列消息中的键盘事件值
#define NativeFldEnterEvent			0x0F00				//事件出列消息中的文本框事件值
//#define EventTypeSize				0x18				//事件结构的尺寸

#define Style1						0x00				//Java、DTG状态的指示样式
#define Style2						0x01				//Java、DTG状态的指示样式
#define Style3						0x02				//Java、DTG状态的指示样式
#define Style4						0x03				//Java、DTG状态的指示样式

#define FORM_UPDATE_FRAMEONLY		0x0001

#define SUGGEST_LIST_HEIGHT			10					//联想列表高度

#endif /* POCKETIME_H_ */
