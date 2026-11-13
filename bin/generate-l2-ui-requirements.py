#!/usr/bin/env python3
# Generate the source-audited UI specification. No serial or firmware mutations.
from pathlib import Path
import hashlib,html,json,re,unicodedata
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'docs/assets/l2-ui-0.5.1'
P={}
def add(k,title,entry,body,back,controls='',state='',kind='list',src='UITask.cpp'):
    buttons=[]
    for line in controls.strip().splitlines():
        if line.strip():
            c=line.strip().split('~');buttons.append(c+['常态可用'] if len(c)==3 else c)
    P[k]=dict(title=title,entry=entry,body=body,back=back,buttons=buttons,state=state,kind=kind,src=src)
add('Home','首页','引导完成；底部Home；主页面左右滑动',
'名称顶栏；电量%+电池；七段时钟：时分、秒数、闪动冒号、LED扫描条、校时提示；单栏Messages、Contacts、Apps、自定义入口；列表底部+。',
'主页面，无返回箭头。','''Messages~Go~Chats；未读总数
Contacts~Go~Contacts；节点数量
Apps~Go~Apps
自定义入口~Go~对应目标，见候选表
底部 +~ShowShortcuts~OV-02快捷入口浮窗''',
'未校时--:--；时钟可见且亮屏约5fps，冒号500ms交替。Apps和+需上下浏览。','clock','TouchUI.cpp')
add('Chats','消息首页','Home → Messages；底部Msg；左右滑动',
'Public channel、Private channel、Contacts三条单栏入口，顶部Add。','主页面，无返回箭头。','''Public channel~Go~PublicChannels
Private channel~Go~PrivateChannels
Contacts~Go~ChatContacts
顶部Add~Go~NewChannel''',src='TouchUI.cpp')
add('Contacts','联系人主页','Home → Contacts；底部People；左右滑动',
'所有类型已保存节点；名称常驻；近期听到绿，否则红；信号格/电量%每1s交替；灰/亮星标；收藏优先。',
'主页面，无返回箭头。','''节点卡~ExpandContact~原位展开/收起，同时只展开一个
星标~ToggleFavorite~收藏切换保存；列表回顶部
展开Message~OpenContact~私聊Conversation~聊天类型节点
顶部Find~Go~Discover
顶部Add~ManualContact~Input：ContactKey → ContactName''',
'普通x10/宽300/高54；展开x4/宽312/高132，显示GPS、SNR、RSSI。15分钟内听到为在线，不是握手；电量遥测1小时内有效，否则--%。信号是最后一跳。展开且TX允许会请求一次遥测，不是每秒轮询。','contacts','TouchUI.cpp')
add('Location','地图主页','底部Map；左右滑动；GPS页；快捷入口；查看轨迹/航点',
'瓦片/栅格；真实/最后位置；层级和底图；GPS状态/卫星数；右上三横线；底部7键；条件出现REC和Exit track。',
'主页面，无返回箭头。','''< / ^ / v / >~MapPan~视图左/上/下/右平移，每次64px
O~MapCenter~居中GPS恢复跟随~至少曾有真实位置
+ / -~MapZoom~缩放一级，3～18
三横线~MapMenu~OV-03地图菜单开关
GPS状态条~Go~GPSStatus
Exit track~UnloadTrack~退出覆盖层，不删除文件~有载入轨迹''',
'当前位置绿实心，旧位置琥珀空心；无坐标不伪造点。SD /map/z/x/y.png，XYZ 256×256 PNG，缺图栅格。地图按方向键移动；左右滑仍切主页。','map','TouchUI.cpp')
add('Settings','设置首页','底部Set；左右滑动','Basic/Advanced两个入口。','主页面，无返回箭头。','''Basic settings~Go~BasicSettings
Advanced settings~Go~AdvancedSettings''',src='TouchUI.cpp')
add('Conversation','会话','频道列表；聊天联系人展开Message；已保存节点详情发消息',
'会话名；频道/私聊；历史卡含发信人、状态、正文预览；收到左对齐、发出右缩进；Quick reply/Write。',
'conversationReturn来源列表，恢复位置。','''消息卡~OpenMessage~MessageDetail
Quick reply~Phrase~Phrases；选择后去Composer
Write~Go~Composer，加载目标草稿''',
'进入标已读并滚到底部。历史全机48条；按密钥摘要/完整公钥隔离。','conversation')
add('Composer','消息编辑','Conversation → Write；短语选择后',
'会话标题；草稿尾部；UTF-8字节数/上限；英文/符号键盘；Aa、123、Space、删除、Emoji、Phrase、Send。',
'保存草稿后回Conversation；失败留页。','''字符键~Key~追加字符
Aa~Shift~大小写
123~KeyboardMode~字母/符号
Space~Space~空格
<-~DeleteChar~删除完整UTF-8字符
Emoji~ShowEmoji~Emoji
Phrase~Phrase~Phrases
Send~Send~成功清草稿回Conversation；失败保留~非空、未超字节限、TX允许''',
'私聊上限MAX_TEXT_LEN；频道扣节点名+分隔符。草稿缓冲192字节。无中文拼音输入法；UI中英文。发送再校验目标。','keyboard')
add('ContactDetail','节点详情','Discover；其他非聊天列表入口',
'节点名；64位公钥分4行；已保存状态；条件出现发消息/加联系人。','detailReturn来源列表。','''Write a message~Write~Conversation；再点Write才编辑~已保存聊天节点
Add contact~AddDiscovered~保存并提示，留本页~未保存''',
'Contacts默认点击是原位展开，不跳本页。')
add('Discover','发现设备','Contacts → Find','最多16条广播设备及收到后秒数；Advertise。','固定Contacts。','''设备卡~OpenDiscovered~ContactDetail
Advertise~Advert~排队广播本机信息并提示~TX允许''',
'被动收集；打开页面不主动扫描；重复公钥更新。',src='TouchUI.cpp')
add('Radio','LoRa地区预设','Basic → LoRa；快捷入口；外部无线电参数变化',
'863～925MHz说明；23条预设滚动列表；频率/BW/SF/CR/路径字节摘要；翻页和确认。','parentPages来源，通常Basic。','''预设项~ChoosePreset~只改draft
< / >~Scroll~翻预设
Confirm band~ApplyRadio~校验保存成功去Home~已选、无待处理/发送任务''',
'选择不立即改电台。非自由填参；功率14dBm；预设快照，不自动选地区。')
add('Quick','快捷操作','Basic → Display & language；Apps；快捷入口',
'亮度+/-、Language、Lock、UTC偏移、Sleep。','previous最近navigate来源，不是完整历史栈。','''Brightness + / -~Brightness~26/64/128/191/255背光档切换保存
Language~Language~English/简体中文切换保存
Lock~Lock~Locked，记录lockReturn
UTC偏移~Timezone~每次+1h，+14后回-12
Sleep~Shutdown~Confirm''',
'旧Close/Back文字按钮被统一抑制，使用左上箭头。','quick')
add('Locked','锁屏','Quick → Lock；60s空闲；Wake侧键',
'未读总数；2张卡/屏，发信人+正文前15个Unicode字符；最新10条；右滑解锁滑块。','无返回/主导航；解锁回lockReturn。','''滑块右拖至末端松手~手势直接处理~恢复锁定前页面
卡区上下滑~手势直接处理~每次2条；下滑看较旧''',
'无未读空态；卡不点进会话。灭屏首次触摸只唤醒。不再长按解锁；可全局截屏。','locked','TouchUI.cpp')
add('About','关于设备','Basic；Apps；快捷入口',
'型号、MeshCore版本、UI 0.5.1、芯片、16MB/8MB；BLE开启才显示PIN。','固定BasicSettings，即使来自Apps。',state='本文不导出实际PIN；固定返回是实现差异。',kind='info')
add('SetupName','引导1 名称语言','首次/固件摘要变化/配置未确认',
'Seeed、Make it yours、名称、English/简体中文、下一步。','引导首屏无返回。','''设备名~EditName~Input NodeName，完成回本页
English / 简体中文~Language~改draft语言
Next~NextSetup~SetupRadio''',
'引导期间禁止TX；引导不等于数据删除。','setup')
add('SetupRadio','引导2 地区预设','SetupName下一步','共用Radio布局，预设/摘要/翻页/确认。','SetupName。','''预设项~ChoosePreset~改draft
< / >~Scroll~翻预设
Confirm band~NextSetup~SetupPrivacy~已选预设''',state='本页只去下一步，不提交。')
add('SetupPrivacy','引导3 定位隐私','SetupRadio确认',
'GPS开关、位置分享关闭说明、无需定位可聊天、Continue。','SetupRadio。','''GPS on/off~Gps~改draft，不立即上电
Continue~NextSetup~SetupFinish''',kind='info')
add('SetupFinish','引导4 确认开始','SetupPrivacy → Continue',
'名称、预设、频段/14dBm、不自动分享位置、Save & start。','SetupPrivacy。','''Save & start~FinishSetup~校验保存并允许TX，去Home~任务空闲且配置/回读通过''',state='失败不允许TX；成功按选择启用GPS。',kind='info')
add('Input','通用输入','名称；手工联系人；新频道',
'类型标题；内容尾部、字节数、键盘、Done/Continue；六变体见专表。','inputReturn；私有密钥取消回NewChannel。','''字符键~Key~追加
Aa~Shift~大小写
123~KeyboardMode~符号
Space~Space~空格
<-~DeleteChar~删除UTF-8字符
Done / Continue~InputDone~校验后下一阶段/来源~非空且类型格式正确''',
'完成时校验长度/十六进制/重复/#前缀。','keyboard')
add('Emoji','表情选择','Composer → Emoji','Smile/Confirmed/Wait/Location/Warning五个名称及对应UTF-8表情。','Composer。','表情项~InsertEmoji~追加草稿，回Composer',state='是名称列表，非图片网格。')
add('Confirm','二次确认','删除轨迹/航点、关闭BLE、管理、Sleep',
'确认文案、影响提示、Cancel/Confirm。','返回等效取消，回confirmReturn。','''Cancel~ConfirmNo~回来源，不执行
Confirm~ConfirmYes~执行confirmedAction，见专表''',
'全屏替换，非浮窗；禁主导航。Cancel绿色，Confirm普通卡色。','confirm')
add('MessageDetail','消息详情','会话消息卡','发信人、状态、正文上下滚动；翻阅键。','Conversation。','< / >~Scroll~前后约100px',state='历史淘汰后提示不在历史中。',kind='info')
add('Phrases','快捷短语','Conversation → Quick reply；Composer → Phrase',
'OK, understood. / On my way. / Please wait. / I have arrived. / Please share your location. 五项。',
'固定Composer，即使来自Conversation。','短语项~ChoosePhrase~追加草稿去Composer，不自动发',state='从Conversation进入先载入目标草稿。')
add('Diagnostics','硬件状态','Advanced；Apps/Home快捷入口',
'I/O扩展器、PSRAM/Heap、内部FS用量、日志健康、SD状态。','parentPages来源。',
'Touch test~Go~TouchTest',state='均为真实硬件值。',kind='info')
add('TouchTest','触控测试','Diagnostics → Touch test',
'触摸四角提示、四个方框、触摸点圆环、X0～319/Y0～239。','固定Diagnostics。',state='仅显示坐标，无重新校准/保存矩阵按钮。',kind='touch')
add('Audio','音频主页','底部Audio；左右滑动；Apps/快捷入口',
'Recording、Music；有音轨选择才显示Now playing。','第5个主页面，无返回。','''Recording~Go~Recordings，扫描/record/*.wav
Music~Go~Music，扫描/music/*.mp3
Now playing~Go~AudioPlayer~已有音轨''',state='播放器回列表不断播放；录音列表返回Audio会停止录制。',src='TouchPages.cpp')
add('PublicChannels','公开频道列表','Chats → Public channel',
'默认Public与已保存#频道；频道名/会话说明；右上+。','固定Chats。','''频道卡~OpenChannel~Conversation
顶部 +~Go~NewChannel''',state='按实际密钥和命名规则分类；总40个频道槽。',src='TouchPages.cpp')
add('PrivateChannels','私有频道列表','Chats → Private channel',
'共享密钥频道；空态提示+；右上+。','固定Chats。','''频道卡~OpenChannel~Conversation
顶部 +~Go~NewChannel''',state='无额外频道密码解锁页。',src='TouchPages.cpp')
add('ChatContacts','消息中的联系人','Chats → Contacts',
'仅聊天类型；收藏优先，名称、遥测交替、原位展开。','固定Chats。','''节点卡~ExpandContact~原位展开/收起
星标~ToggleFavorite~收藏保存
展开Message~OpenContact~私聊Conversation
顶部 +~Go~NewChannel（当前实现）''',state='此+不是新增联系人；手工添加在Contacts → Add。',kind='contacts',src='TouchPages.cpp')
add('BasicSettings','基础设置','Settings → Basic',
'依次LoRa、显示语言、蓝牙、名称、提示音、音量+、音量-、关于，上下浏览。','固定Settings。','''LoRa configuration~Go~Radio
Display & language~Go~Quick
Bluetooth~Bluetooth~关闭时直接开；已开时Confirm关闭~USB版仅提示需BLE版
Device name~EditName~Input NodeName
Sound~SoundToggle~静音开关保存；启用可播ACK音
Volume + / -~AudioVolume~每次±10%，0～100%，保存
About~Go~About''',src='TouchPages.cpp')
add('AdvancedSettings','高级设置','Settings → Advanced',
'GPS、Hardware status、红色Administration。','固定Settings。','''GPS~Go~GPSStatus
Hardware status~Go~Diagnostics
Administration（红色）~Go~Administration''',src='TouchPages.cpp')
add('Recordings','录音列表/录制','Audio → Recording',
'/record/*.wav；LATEST最新置顶；录制时长与输入电平；单一Record按钮。','固定Audio；在录则停止保存。','''Record~RecordAudio~开始录制；按钮变波形+Press again to stop~SD可写、≥1MB空间、无音频占用
波形+再按一次停止~RecordAudio~保存并刷新列表回顶部
录音文件卡~PlayAudio~AudioPlayer原生WAV回放~录制时禁用''',
'16kHz/16位/单声道；最长30min；每秒文件头检查点；.wav.part完成改.wav；列表最多128。旧文件不改；无独立Stop键。','recorder','TouchPages.cpp')
add('Music','音乐文件列表','Audio → Music；快捷入口',
'/music/*.mp3列表；底部Stop。','固定Audio。','''文件卡~PlayAudio~开始播放并去AudioPlayer
Stop~StopAudio~停止、刷新列表，留本页''',state='MP3不是MP4；无删除/重命名/播放列表编辑。',src='TouchPages.cpp')
add('NewChannel','新增频道类型','Chats Add；列表+；快捷入口',
'Public #channel / Private channel两卡。','固定Chats。','''Public #channel~AddChannel~Input PublicChannelName，初值#；成功回PublicChannels
Private channel~AddChannel~Input PrivateChannelName → ChannelKey；成功回PrivateChannels''',
'创建成功先回对应频道列表，再点击才进会话。名/密钥重复、格式不合法、满额时提示，不覆盖。',src='TouchPages.cpp')
add('AudioPlayer','正在播放','Music/Recordings点击文件；Audio → Now playing',
'文件名、状态/错误/近静音；真实PCM波形；已播/总长；进度；上一首/暂停/下一首/Stop；音量滑杆/百分比/±。',
'按所选音轨类型回Music/Recordings，播放继续。','''上一首 / 下一首~PlayerPrevious / PlayerNext~当前库前后曲、首尾循环
Play / Pause~PlayerPause~暂停/继续；停止后重新开始
Stop~StopAudio~停止，保留选择和本页
进度点击/拖动~PlayerSeek~松手定位，不触发切页
音量滑杆~PlayerVolume~实时0～100%，松手保存
- / +~AudioVolume~每次±10%并保存''',
'WAV按PCM帧；MP3字节/帧近似定位，VBR不承诺精确秒。估算总长加~。近静音明确提示。','player','TouchPages.cpp')
add('Tracks','轨迹管理','地图菜单Track；Apps；快捷入口',
'新增/停止记录单按钮；CSV列表；空态仅记录真实定位。','parentPages来源。','''+ New track~TrackRecord~必要时启用GPS并新建文件，留本页~SD可写；无定位等待
Stop track recording~TrackRecord~结束记录保留文件
轨迹卡~OpenTrack~TrackDetail''',
'/tracks/track-xxxxxxxx.csv；每5s新有效样本；无fix不写0,0；列表最多32。独立于麦克风录音。',src='TouchShell.cpp')
add('TrackDetail','轨迹操作','Tracks点击轨迹','文件名、Load on map、红色Delete。','固定Tracks。','''Load on map~LoadTrack~Location，出现Exit track
Delete track~DeleteTrack~Confirm；成功删除回Tracks~同一正在记录轨迹禁止删''',
'读取≤4MB，预览抽稀≤512点，完整CSV不改。无有效点不能载入。',src='TouchShell.cpp')
add('Waypoints','航点列表','地图Waypoint；Apps；快捷入口',
'保存当前位置；名称/坐标卡；无fix时主按钮改打开GPS。','parentPages来源。','''+ Add current position~AddWaypoint~保存真实定位~有效fix且未满24
GPS fix needed~Go~GPSStatus~无有效定位
航点卡~OpenWaypoint~WaypointDetail''',
'/waypoints.csv，默认Waypoint N；无自由输入经纬度/改名入口。',src='TouchShell.cpp')
add('WaypointDetail','航点操作','Waypoints点击航点',
'航点名、View on map、红色Delete。','固定Waypoints。','''View on map~ShowWaypoint~Location居中保存坐标
Delete waypoint~DeleteWaypoint~Confirm；成功回Waypoints''',src='TouchShell.cpp')
add('Tiles','离线瓦片','地图Tile；Apps → Tiles',
'开关、/map/z/x/y.png、当前中心路径、缩放±、Rescan SD。','parentPages来源。','''Offline tiles on/off~ToggleTiles~持久开关并使地图重绘
Zoom - / +~MapZoom~调整地图缩放，留本页
Rescan SD~RescanMap~重读相关地图数据/轨迹/航点并提示''',
'256×256 PNG，≤512KB/张，z3～18。无下载、联网、MBTiles入口；不再读/tiles，不自动移动旧文件。',src='TouchShell.cpp')
add('GPSStatus','GPS状态','Apps；Advanced GPS；地图条；罗盘；无fix航点入口',
'开关/重启；关/等串口/识别波特率/搜星/定位；可见/使用星数；波特率/字节/NMEA数；坐标；Open map。','parentPages来源。','''Turn on/off~Gps~电源切换并保存
Restart GPS~ResetGps~复位、清有效状态和旧航向~已开启
Open map~Go~Location''',
'无位置不编造坐标；旧位置与当前fix分开。RX18/TX17。',kind='info',src='TouchShell.cpp')
add('Apps','应用列表','Home → Apps',
'顺序9项：Audio、GPS status、Tracks、Waypoints、Tiles、Quick panel、About、Compass、Screenshots；上下滑。','parentPages来源，通常Home。','''Audio~Go~Audio
GPS status~Go~GPSStatus
Tracks~Go~Tracks
Waypoints~Go~Waypoints
Tiles~Go~Tiles
Quick panel~Go~Quick
About~Go~About
Compass~Go~Compass
Screenshots~Go~ScreenshotSettings''',state='不是第7个横滑主页面。',src='TouchShell.cpp')
add('Administration','管理操作','Advanced → 红色Administration',
'红色标题和Factory reset、Reboot、Shut down三项。','parentPages，通常Advanced。','''Factory reset~FactoryReset~Confirm；清内部身份/联系人/配置、重启，SD保留
Reboot~Reboot~Confirm；保存草稿、停任务、提示音后重启
Shut down~Shutdown~Confirm；保存/停任务/提示音/低功耗关机''',
'Shut down是深睡眠而非物理断电，BOOT唤醒。本文采图未执行管理操作。',src='TouchShell.cpp')
add('Compass','GPS行进罗盘','Apps → Compass',
'N/E/S/W刻度、方向针、角度、速度；GPS按钮；明确无磁力计。','parentPages，通常Apps。','''Enable GPS~Gps~开启GPS保存，留本页~GPS关闭时
GPS status~Go~GPSStatus~GPS开启时''',
'有效定位且RMC航向≤5s、速度≥1节约1.852km/h才显示。静止/无效-- deg、Move to read。测行进方向，不测静止设备朝向。','compass','TouchTools.cpp')
add('ScreenshotSettings','截屏设置','Apps → Screenshots',
'开关；长按左上1.2s说明；/screenshots/*.bmp；SD和Tap to dismiss。','parentPages，通常Apps。','''Screenshots on/off~ToggleScreenshots~持久保存开关，默认关
全局左上长按~无Action：直接capture~保存BMP；成功弹窗OV-05~已开，x0～39/y0～29保持≥1.2s，无移动''',
'一次按下最多一张；短按返回仍可用；冻结帧/分块写，完成重命名才提示；无卡/失败报错。320×240/24位BMP。','info','TouchShell.cpp')
INPUTS=[('NodeName','设备名称','31字节','引导改draft；普通提交保存后回inputReturn。'),('ContactKey','联系人公钥','64位十六进制','格式、长度、重复检查通过后原页转ContactName。'),('ContactName','联系人名称','31字节','默认Peer+公钥尾8字符；成功回Contacts。'),('PublicChannelName','公开频道名','31字节','必须#开头且后续非空；派生密钥，成功回PublicChannels。'),('PrivateChannelName','私有频道名','31字节','完成原页转ChannelKey。'),('ChannelKey','私有频道密钥','32位十六进制','转16字节密钥；成功回PrivateChannels。')]
OVERLAYS=[
('OV-01','底部主导航','底边上滑；BOOT短按','300×58，x10/y182；Home/Msg/People/Map/Audio/Set；层级高于页面与局部菜单。','''六个入口~Go~对应主页面，关抽屉
向下滑/窗外点~手势直接关闭~仅关闭，不穿透
5s空闲~手势计时处理~自动隐藏，按住不计超时'''),
('OV-02','首页快捷入口浮窗','Home底部+','304×197，4项/屏，上下滑看11候选；勾表示已添加；最多8项。','''候选项~ToggleShortcut~添加/移除；保存成功关窗并滚到底
右上x/窗外点~CloseShortcuts~关闭，不切页'''),
('OV-03','地图竖向菜单','地图右上三横线','x174/y73/139×124；Track/Waypoint/Tile，icon+名称。','''Track / Waypoint / Tile~Go~Tracks / Waypoints / Tiles
窗外点击~CloseMapMenu~仅关闭，不点穿'''),
('OV-04','普通状态提示','保存/广播排队/蓝牙等','顶端312×108，Tap to dismiss；约2s自动过期；压在菜单之上。','点击任意处~Help~关闭提示，不执行下面按钮'),
('OV-05','截屏成功提示','BMP成功写完并重命名','Screenshot saved/截屏成功、实际路径、Tap to dismiss；不自动消失；锁屏也可关闭。','点击任意处~Help~只关提示，原页面保持'),
('OV-06','错误提示','参数/存储/SD/音频/发送失败','棕底/琥珀字，Tap to dismiss；不自动消失；全屏捕获点击。','点击任意处~Help~关闭，不穿透')]
HEADER='''# Wio Tracker L2 Pro UI 需求与交互规格

> Seeed UI development **0.5.1** 实现基线；2026-11-12，Asia/Shanghai。
>
> 本文逐项描述当前源码已经实现的页面、功能键、跳转、返回和堆叠，可作为后续开发与验收基线。不是把历史设想当作全部已实现。当前限制、固定返回差异与待验收项单列。
>
> **实机截图**来自COM61；**源码结构图**为解释布局的图示，不是设备截图或像素级复刻。动态数据使用占位，不伪造节点、消息、定位、轨迹、音频或硬件验收。

## 1. 范围与资料

- 横屏320×240，界面中英文；USB与USB+BLE共用UI。
- 覆盖44个Page、74个Action、6种输入变体、6种覆盖层。
- 基线BLE应用SHA-256：`96b022704ee44452894fc9637d8df59e837e4a96fe1cb9d23c65011794412450`。
- 本次仅连接串口读取画面/状态；未发送消息/广播、删除用户文件、恢复出厂、刷固件或构造测试节点。
- 主源码：[UITask.h](../examples/companion_radio/ui-seeed/UITask.h)、[UITask.cpp](../examples/companion_radio/ui-seeed/UITask.cpp)、[TouchUI.cpp](../examples/companion_radio/ui-seeed/TouchUI.cpp)、[TouchPages.cpp](../examples/companion_radio/ui-seeed/TouchPages.cpp)、[TouchShell.cpp](../examples/companion_radio/ui-seeed/TouchShell.cpp)、[TouchInput.cpp](../examples/companion_radio/ui-seeed/TouchInput.cpp)、[TouchTools.cpp](../examples/companion_radio/ui-seeed/TouchTools.cpp)。
- 数据/硬件：[LocalStore.h](../examples/companion_radio/ui-seeed/LocalStore.h)、[L2Map.cpp](../variants/wio-tracker-l2-pro/L2Map.cpp)、[L2Audio.cpp](../variants/wio-tracker-l2-pro/L2Audio.cpp)、[L2Capture.cpp](../variants/wio-tracker-l2-pro/L2Capture.cpp)、[L2Sensors.h](../variants/wio-tracker-l2-pro/L2Sensors.h)。
- 可视化总览：[全部页面图集](l2-ui-gallery.html)，支持浏览器和打印；逐页详细要求仍以本文为准。
- 重新生成：`python bin/generate-l2-ui-requirements.py`；生成器不连接设备。枚举变化会要求更新规格。

## 2. 实机图与验收状态

### 2.1 首页实机截图

![首页：COM61真实帧缓冲](assets/l2-ui-0.5.1/device-home.png)

### 2.2 锁屏实机截图

![锁屏：COM61真实帧缓冲](assets/l2-ui-0.5.1/device-current.png)

两张图已实际读取并查看。当前固件只能读当前画面，无远程切页接口；其余页面附源码结构图，并非全页面实拍。英语实机图与中文说明结构图可能文案不同。USB图为PNG；设备保存的截屏为BMP，不能把一次USB读图当成SD截图文件的内容验证。

| 项目 | 状态/依据 |
| --- | --- |
| 0.5.1启动、切页、时钟与动效 | 用户确认“切页和时钟均正常” |
| 截屏开关、左上长按、成功提示、点击关闭 | 用户确认“成功提示和点击关闭均正常” |
| 身份与存储 | 本次只读核对与升级前身份一致；内部60/3456KiB；SD/codec就绪 |
| 首页/锁屏画面 | COM61读取320×240帧缓冲，已查看图像 |
| GPS定位、录音回放、音量 | 0.5.0曾实机确认；不替代0.5.1所有场景重复验收 |
| 仍待专项测试 | 全页面视觉、多节点遥测、真实瓦片、轨迹/航点增删加载、长期掉电 |

## 3. 全局布局、手势与层级

### 3.1 布局与视觉

| 区域 | 规格 |
| --- | --- |
| 画布 | 320×240，左上原点 |
| 顶栏 | y0～29，深绿色；左标题，右百分比+电池；必要时TX LOCK或指示点 |
| 主列表 | 通常y34～213；x10/宽300/高54卡片，纵间隔60；特殊页自定义裁剪视窗 |
| 底部提示 | 仅6个主页y215～239：左滑下一页、6页点、上滑菜单；不是一直展开的导航 |
| 统一返回 | 子页左上x4/y2/29×26橙色圆角箭头，只icon；旧Back文字按钮不绘制、不响应 |
| 配色 | 背景约#0B1214、卡#162429、强调#BEED35、抽屉#003A4A、顶栏#284B32；橙色返回；红色管理操作/未近期听到名称 |
| 字体 | 常规16px、辅助14px、标题/按钮加粗，大字24px；首页数字46px七段图形 |
| 电量 | 暂用3300mV=0%、4200mV=100%线性估算；未知--%；百分比+电池，低电量强调 |
| 刷新 | 差分行刷新；最低约33ms重绘间隔；音频页/录音约10fps；可见时钟/GPS/罗盘约5fps |

### 3.2 手势与实体键

| 输入 | 行为 |
| --- | --- |
| 主页面左滑 | Home → Chats → Contacts → Location → Audio → Settings → Home |
| 主页面右滑 | 反向循环；子页不按左右滑切主页面 |
| 内容上下滑 | 当前列表/正文滚动；移动后不当作单击 |
| 点击 | 按下/松手同一有效控件区域才触发 |
| 底边上滑 | y≥216起点、上移≥28px，打开导航；引导/Locked/Confirm禁止 |
| 导航关闭 | 向下滑、空白点、选中项或5s无操作；触摸按住时不超时 |
| 播放滑杆 | 捕获拖动；进度松手定位，音量实时变化且松手保存；不误切页 |
| 锁屏解锁 | 从左端滑块右拖至末端松手；纵偏过大取消；非长按 |
| 锁屏卡浏览 | 上下滑一次2条；下看更旧；最多最新10条 |
| 截屏 | 开关开后，左上40×30区域长按≥1.2s，无移动；单次按下一张；短按返回不变 |
| 空闲30s | 关屏；首次触摸只点亮 |
| 空闲60s | Locked并保存返回页 |
| BOOT/GPIO0 | 正常亮屏非引导/锁屏：短按切导航，长按返回；也用于深睡唤醒 |
| 扩展器Wake侧键 | 亮屏时锁屏并熄屏，灭屏时点亮；去抖且不把触摸当侧键 |

### 3.3 堆叠与返回模型

![覆盖层堆叠结构](assets/l2-ui-0.5.1/layers.svg)

从低到高：当前页面背景/顶栏/正文 → 统一返回箭头 → 首页快捷入口或地图菜单 → 底部主导航 → 普通/错误/截屏提示。

浮窗打开后重建点击区域；窗外点只关闭，底部导航不点穿；提示窗高于菜单，点击仅关闭提示。Confirm与Locked是页面替换，不是再叠一层透明弹窗；播放器和输入页也为子页面。

实现用page、parentPages[64]、previous及特定return字段，**不是通用无限历史栈**。每种Page类型仅一个父级槽及一个滚动位置；重入同类页会更新来源。下文“返回”列是实际代码，而非理想化的全部原路返回。

### 3.4 主要跳转

```mermaid
flowchart TD
  SetupName --> SetupRadio --> SetupPrivacy --> SetupFinish --> Home
  Home --> Chats
  Home --> Contacts
  Home --> Apps
  Chats --> PublicChannels --> Conversation
  Chats --> PrivateChannels --> Conversation
  Chats --> ChatContacts --> Conversation
  Chats --> NewChannel --> Input
  Input --> PublicChannels
  Input --> PrivateChannels
  Contacts --> Discover --> ContactDetail
  Conversation --> Composer
  Conversation --> MessageDetail
  Conversation --> Phrases --> Composer
  Composer --> Emoji --> Composer
  Apps --> Audio
  Apps --> GPSStatus
  Apps --> Tracks
  Apps --> Waypoints
  Apps --> Tiles
  Apps --> Compass
  Apps --> ScreenshotSettings
  Location --> MapMenu
  MapMenu --> Tracks --> TrackDetail --> Location
  MapMenu --> Waypoints --> WaypointDetail --> Location
  MapMenu --> Tiles
  Audio --> Recordings --> AudioPlayer
  Audio --> Music --> AudioPlayer
  Settings --> BasicSettings --> Radio
  Settings --> AdvancedSettings --> Administration --> Confirm
  BasicSettings --> Quick --> Locked
  AdvancedSettings --> Diagnostics --> TouchTest
```

Input目标依六种模式而异；新增频道成功先回频道列表。6个主页面通过底部导航互达。

## 4. 页面目录
'''
TAIL='''
## 8. 公共数据、音频与文件约束

| 数据/文件 | 位置或范围 | UI约束 |
| --- | --- | --- |
| 设置 | 内部LocalStore、校验及固件摘要 | 名称/语言/时区/背光/GPS/预设持久化；新固件需通信确认 |
| 联系人/频道 | 内部存储；构建350联系人/40频道槽 | 收藏保存；身份按完整公钥/密钥；Public默认存在不覆盖旧频道 |
| 历史 | 全机48条 | 锁屏最新10未读只是筛选；不是每会话48条 |
| 草稿 | 最多12目标，每份192字节 | 按目标身份保存；失败保留并提示；不会仅凭名称串话 |
| 快捷入口 | NVS l2-home | 最多8，从固定11选；校验回读，失败回滚 |
| 截屏开关 | NVS l2-ui | 默认关；重启保持，恢复出厂清除 |
| 音量 | NVS l2-audio，0～100% | 默认35%；±10%；拖动实时，松手保存 |
| 瓦片 | SD /map/z/x/y.png | XYZ/WebMercator、256×256 PNG、单张≤512KB、z3～18；缺图栅格，无自动下载 |
| 轨迹 | SD /tracks/*.csv | 5s有效样本；最多32条列表；载入≤4MB/预览512点；完整文件保留 |
| 航点 | SD /waypoints.csv | 最多24；真实有效定位新增；临时/备份文件保存保护 |
| 录音 | SD /record/*.wav（中间.wav.part） | 16kHz/16位/单声道，最长30min，每秒检查点；LATEST置顶 |
| 音乐 | SD /music/*.mp3 | 最多128文件条目；非MP4；无管理文件按钮 |
| 截屏 | SD /screenshots/L2-xxxxxxxx.bmp | 320×240/24位；不覆盖旧图；写完并重命名后提示 |
| SD失败 | 不自动格式化 | 明确报错，其他可用页面继续；无一键清空SD |

提示音含开机、软件关机/休眠、发送、私聊/频道接收、ACK，音符参考MeshCore原版。录音、回放与提示音互斥；工作时不抢I2S插音，消息界面仍更新。物理断电不能播放关机音。

消息状态按实际过程显示Received、Queued、Transmitted、Delivered/Recipient ACK、Unconfirmed、Failed；超时不当作成功。频道无私聊式送达ACK；提交失败保留草稿。

## 9. 当前实现差异（不是在本文中顺便改固件）

1. About固定回Basic；NewChannel固定回Chats；Phrases固定回Composer；Quick只用previous，不是任意深度历史返回。
2. ChatContacts顶部+去NewChannel，手工加联系人须Contacts → Add。
3. Contacts点击原位展开；显示在线是15分钟最近听到；遥测缺失不补假电量，信号不是直接端到端链路值。
4. Compass是GPS行进罗盘；无磁力计，不能验收为静止磁北指南针。
5. 截屏成功与错误提示常驻；普通成功约2s。USB读PNG和SD保存BMP是两条通路。
6. 输入键盘无拼音；瓦片不支持MBTiles/联网下载；LoRa是预设选择而非自由参数编辑；航点没有改名/任意坐标输入。
7. 44个Page不代表44层历史栈；同类页面共用父级/滚动槽；录音和音乐共用文件库状态。
8. 字体/长文件名/两种语言截断、弹窗边缘、各种动态数据状态仍应逐页实测。结构图仅是说明，不是所有状态已验收。

## 10. 后续验收清单

| 类别 | 场景 |
| --- | --- |
| 导航 | 六页方向与循环、子页不横滑换主页面、抽屉防穿透/5s、禁用场景 |
| 返回 | 每页返回列、多入口、录制时返回、草稿失败、Confirm取消 |
| 首页 | 未校时/校时/时区、动效无黑闪、Apps/+可达、8项快捷入口重启保存 |
| 消息/联系人 | Public、两类频道新增/重复/满额、手工公钥校验、草稿/投递/未确认、收藏/在线过期/缺遥测 |
| 地图 | /map真瓦片和缺图、平移/缩放/回中、旧位置、轨迹记录/载入/退出/确认删除、航点保存/查看/删除 |
| 音频 | 单按钮录制/时长/电平/LATEST、新WAV/MP3、暂停停止/首尾切曲/拖进度/音量、静音与损坏格式 |
| 截屏 | 关时不触发、开时1.2s一次、短按返回/移动取消、实际BMP正色与方向、无卡错误、锁屏/菜单提示可关闭 |
| 罗盘/GPS | 无串口/搜星/fix、移动/静止/航向过期，不伪造方向 |
| 设置/管理 | 持久化、USB/BLE差异；管理操作只在专项测试设备执行 |

## 11. Action完整索引

以下对照UITask.h全部74项；Action不是USB接口。某些操作如锁屏滑块、长按截图和拖动条由触摸层直接调用方法，并非额外按钮。
'''
BG='#0b1214';CARD='#162429';GREEN='#beed35';TEAL='#003a4a';WHITE='#f4f7f6';MUTED='#a4b4b7'
def esc(t):return html.escape(str(t),quote=True)
def fit(t,n=38):
    out='';count=0
    for c in t:
        count+=2 if unicodedata.east_asian_width(c) in 'WF' else 1
        if count>n:return out+'…'
        out+=c
    return out
class Svg:
    def __init__(self,h=270):
        self.h=h;self.p=[f'<svg xmlns="http://www.w3.org/2000/svg" width="320" height="{h}" viewBox="0 0 320 {h}" role="img"><title>源码结构图，非实机截图</title>',f'<rect width="320" height="{h}" fill="{BG}"/>','<g font-family="Microsoft YaHei, Noto Sans CJK SC, sans-serif">']
    def rect(self,x,y,w,h,color=CARD,r=6):self.p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{color}"/>')
    def text(self,x,y,t,color=WHITE,size=13):self.p.append(f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}">{esc(t)}</text>')
    def line(self,x,y,xx,yy,color=TEAL):self.p.append(f'<path d="M{x} {y}L{xx} {yy}" stroke="{color}" fill="none"/>')
    def save(self,name):
        self.text(8,self.h-10,'源码结构示意 · 非实机截图/非像素还原',MUTED,10)
        (ASSETS/name).write_text(''.join(self.p)+'</g></svg>\n',encoding='utf-8')
def wire(k,d,root=False):
    s=Svg();s.rect(0,0,320,30,'#284b32',0)
    if not root:s.rect(4,2,29,26,'#ffa000');s.text(10,22,'←',BG,22)
    s.text(10 if root else 42,21,fit(d['title'],25),GREEN,14);s.text(252,20,'--% ▯',MUTED,12)
    kind=d['kind'];buttons=d['buttons']
    if kind=='clock':
        s.rect(10,34,300,78);s.text(22,49,'L2 // LOCAL TIME',MUTED,11);s.text(24,95,'--:--',GREEN,44);s.text(223,83,'秒',WHITE,18)
        for i in range(12):s.rect(22+i*13,105,9,2,GREEN if i==4 else TEAL,0)
        for y,t,detail in [(122,'Messages','未读数量（动态）'),(182,'Contacts','节点数量（动态）')]:s.rect(10,y,300,54);s.text(20,y+22,t);s.text(20,y+44,detail,MUTED,11)
    elif kind=='keyboard':
        s.rect(8,32,304,40,TEAL);s.text(15,57,'输入内容 / 草稿尾部（动态）',WHITE,12);s.text(12,86,'字节数 / 上限',MUTED,11)
        for r,keys in enumerate(['qwertyuiop','asdfghjkl','zxcvbnm']):
            for i,c in enumerate(keys):s.rect(5+i*31,95+r*29,28,26);s.text(13+i*31,113+r*29,c,WHITE,12)
        for x,w,t in [(5,44,'Aa'),(53,44,'123'),(101,99,'Space'),(204,48,'←'),(256,59,'Emoji' if k=='Composer' else 'Done')]:s.rect(x,183,w,26);s.text(x+6,201,t,WHITE,11)
        s.rect(92,213,223,25,GREEN);s.text(105,231,'Phrase / Send' if k=='Composer' else 'Continue',BG,12)
    elif kind=='map':
        for x in range(0,320,32):s.line(x,34,x,214)
        for y in range(34,215,32):s.line(0,y,320,y)
        s.rect(7,38,130,30);s.text(13,58,'Exit track（有载入）',WHITE,10);s.rect(275,38,35,30);s.text(282,59,'☰',GREEN,22)
        s.text(65,95,'真实位置/轨迹覆盖层',MUTED,12);s.text(12,128,'层级 / 瓦片或栅格',MUTED,11);s.rect(7,144,205,29);s.text(12,163,'GPS状态 / 卫星数（动态）',WHITE,11)
        for i,t in enumerate(['←','↑','↓','→','◎','+','−']):s.rect(8+i*43,180,38,30);s.text(18+i*43,200,t,WHITE,15)
    elif kind=='locked':
        for y,t,detail in [(38,'未读卡1：发信人','前15字符（有未读时）'),(108,'未读卡2：发信人','无未读时改空态')]:s.rect(10,y,300,62);s.text(20,y+22,t);s.text(20,y+47,detail,MUTED,11)
        s.text(12,185,'下滑查看更多：最新10条',MUTED,11);s.rect(12,192,296,40,TEAL,20);s.rect(16,196,42,32,GREEN,15);s.text(30,219,'›',BG,22);s.text(76,217,'向右滑动解锁',WHITE,14)
    elif kind=='contacts':
        for y in [36,96]:s.rect(10,y,300,54);s.text(20,y+22,'节点名称（动态）',GREEN,14);s.text(20,y+44,'信号格 / 电量% 每1s交替',MUTED,11);s.text(280,y+28,'☆',MUTED,20)
        s.text(12,176,'点击原位展开：GPS、SNR、RSSI',MUTED,11);s.text(12,201,'收藏优先；名称按最近听到变色',MUTED,11)
    elif kind=='recorder':
        s.text(12,49,'目录 / 录制时长 / 输入电平',MUTED,11)
        for y,t in [(59,'LATEST：最新录音'),(119,'其他WAV文件')]:s.rect(10,y,300,54);s.text(20,y+22,t);s.text(20,y+43,'点击 → 播放器',MUTED,11)
        s.rect(10,198,300,37,GREEN);s.text(23,221,'Record ↔ 波形 + 再按一次停止',BG,13)
    elif kind=='player':
        s.text(12,49,'文件名（动态）',GREEN,14);s.text(12,70,'播放/暂停/停止/静音提示/错误',MUTED,11);s.rect(10,78,300,38)
        for i in range(30):s.line(16+i*9,97,25+i*9,97+(i%3-1)*7,GREEN)
        s.text(12,132,'已播',MUTED,11);s.text(250,132,'总长',MUTED,11);s.rect(12,141,296,5,TEAL,2)
        for x,w,t in [(10,55,'|<'),(70,96,'Play/Pause'),(171,55,'>|'),(231,79,'Stop')]:s.rect(x,158,w,34);s.text(x+8,180,t,WHITE,11)
        s.text(15,221,'−    --%',WHITE,13);s.rect(100,213,160,5,TEAL,2);s.text(289,221,'+',WHITE,15)
    elif kind=='compass':
        s.p.append(f'<circle cx="105" cy="122" r="60" fill="none" stroke="{MUTED}"/>')
        for x,y,t in [(100,48,'N'),(174,125,'E'),(100,202,'S'),(24,125,'W')]:s.text(x,y,t,GREEN,13)
        s.text(190,111,'-- deg',MUTED,17);s.text(190,139,'移动后读数',WHITE,12);s.rect(190,156,120,34);s.text(200,178,'GPS / 开启GPS',WHITE,11);s.text(12,219,'GPS行进方向 · 无磁力计',MUTED,11)
    elif kind=='conversation':
        s.text(84,49,'频道 / 私聊',MUTED,11)
        s.rect(10,65,300,54);s.text(20,86,'发信人 / 状态（动态）',MUTED,11);s.text(20,108,'收到的消息正文预览',WHITE,13)
        s.rect(34,125,276,54);s.text(44,145,'本机 / 状态（动态）',MUTED,11);s.text(44,166,'发出的消息正文预览',WHITE,13)
        s.rect(10,194,145,40);s.text(25,220,'Quick reply',WHITE,13);s.rect(165,194,145,40,GREEN);s.text(210,220,'Write',BG,14)
    elif kind=='touch':
        s.text(14,48,'触摸四角，显示X/Y',MUTED,13)
        for x,y in [(5,55),(283,55),(5,160),(283,160)]:s.rect(x,y,32,32,TEAL,0)
        s.text(55,126,'X / 319   Y / 239',WHITE,14)
    elif kind=='setup':
        s.text(12,66,'Make it yours.',WHITE,24);s.text(12,88,'Wio Tracker L2 Pro / MeshCore',MUTED,12)
        s.rect(10,108,300,40);s.text(20,133,'设备名称（可编辑）',WHITE,14)
        s.rect(10,158,145,37);s.text(25,182,'English',WHITE,14);s.rect(165,158,145,37);s.text(180,182,'简体中文',WHITE,14)
        s.rect(10,207,300,30,GREEN);s.text(22,228,'下一步：地区与预设',BG,13)
    elif kind=='quick':
        for i,t in enumerate(['亮度 +','亮度 −','Language','Lock','UTC 时区','Sleep']):x=10+i%2*155;y=37+i//2*51;s.rect(x,y,145,42);s.text(x+10,y+26,t)
    elif kind=='confirm':
        s.text(12,65,'具体确认事项（动态）',WHITE,17);s.text(12,113,'影响说明 / 保留内容',MUTED,13);s.text(12,148,'This changes your device.',MUTED,11)
        s.rect(10,173,145,52,GREEN);s.text(52,205,'Cancel',BG,15);s.rect(165,173,145,52);s.text(203,205,'Confirm',WHITE,15)
    else:
        if kind in ['info','touch']:
            for i,t in enumerate(re.findall('.{1,21}',d['body'])[:4]):s.text(12,52+i*22,t,WHITE if i==0 else MUTED,12)
            display=buttons[:2];start=155;step=38;height=33
        else:display=buttons[:3];start=37;step=58;height=52
        for i,c in enumerate(display):
            y=start+i*step;s.rect(10,y,300,height);s.text(20,y+21,fit(c[0]),WHITE,13)
            if height>40:s.text(20,y+42,fit(c[2]),MUTED,11)
        if len(buttons)>len(display):s.text(14,211,'上下浏览更多项；完整功能见表',MUTED,10)
    if k in {'Home','Chats','Contacts','Location','Settings','Audio'}:s.rect(0,215,320,25,TEAL,0);s.text(10,232,'左滑下一页',MUTED,11);s.text(136,232,'● ● ● ● ● ●',GREEN,9);s.text(246,232,'上滑菜单',GREEN,11)
    s.save(k+'.svg')
def buttons_table(buttons):
    out=['| 功能键/触发 | Action | 结果/跳转 | 条件 |','| --- | --- | --- | --- |']
    for row in buttons:
        assert len(row)==4,row
        out.append('| '+' | '.join(str(x).replace('|','&#124;') for x in row)+' |')
    return '\n'.join(out)+'\n'
def overlay_image(o):
    key,title,entry,body,controls=o;s=Svg();s.rect(0,0,320,240,CARD,0);s.text(12,26,'原页面保持原处（结构示意）',MUTED,12)
    if key=='OV-01':
        s.rect(10,182,300,58,TEAL,10)
        for i,t in enumerate(['Home','Msg','People','Map','Audio','Set']):s.rect(12+i*50,194,46,40);s.text(15+i*50,219,t,WHITE,9)
    elif key=='OV-02':
        s.rect(8,35,304,197,TEAL,12);s.text(19,61,'Home shortcuts',GREEN,16);s.text(281,60,'×',WHITE,20)
        for i,t in enumerate(['地图','音频','轨迹','航点']):s.rect(16,75+i*34,288,30);s.text(24,95+i*34,t)
        s.text(19,225,'上下滑动 · 已添加点击可移除',MUTED,11)
    elif key=='OV-03':
        s.rect(174,73,139,124,TEAL)
        for i,t in enumerate(['⌁ Track','◇ Waypoint','▦ Tile']):s.rect(180,80+i*36,126,32);s.text(187,101+i*36,t)
    else:
        s.rect(4,4,312,108,'#6d3920' if key=='OV-06' else TEAL);s.text(12,30,title,GREEN,16);s.text(12,58,'实际文件路径' if key=='OV-05' else '操作结果 / 错误说明（动态）',WHITE,12);s.text(12,104,'Tap to dismiss',MUTED,12)
    s.save(key+'.svg')
# Product-facing output: internal identifiers stay in the generator, never in the document.
def product_text(t):
    t=str(t)
    replacements={
      '源码结构示意':'页面结构示意','源码结构图':'页面结构图','源码结构':'页面结构','源码':'页面',
      'conversationReturn来源列表':'进入会话前的列表','detailReturn来源列表':'进入详情前的列表',
      'parentPages记录的来源':'进入本页前的页面','parentPages来源':'进入本页前的页面',
      'parentPages，通常':'进入本页前的页面，通常为','parentPages':'进入本页前的页面',
      'previous最近navigate来源，不是完整历史栈。':'返回进入快捷操作前的页面；连续跨页使用后的返回体验仍需统一。',
      'inputReturn':'输入前的页面','lockReturn':'锁定前的页面','confirmReturn':'发起确认的页面',
      '只改draft':'仅暂存更改，最终确认时保存','改引导draft':'暂存引导设置','改draft':'暂存设置',
      'draft':'尚未保存的设置','固件摘要变化':'更新后需要重新确认设置',
      'MAX_TEXT_LEN':'160字节','UTF-8':'文字','TX允许':'允许发送消息','TX禁止':'禁止发送消息',
      '禁止TX':'禁止发送消息','允许TX':'允许发送消息','RMC':'GPS','fix':'定位结果',
      '持久保存':'保存并在重启后保留','持久':'保存并在重启后保留','槽':'名额',
      'OV-01':'底部导航','OV-02':'快捷入口选择窗','OV-03':'地图菜单','OV-04':'普通提示',
      'OV-05':'截屏成功提示','OV-06':'错误提示','当前实现':'当前版本','代码':'当前版本',
    }
    for a,b in sorted(replacements.items(),key=lambda pair:-len(pair[0])):t=t.replace(a,b)
    names={k:d['title'] for k,d in P.items()}
    names.update({k:title for k,title,_,_ in INPUTS})
    names.update({'Basic':'基础设置','Advanced':'高级设置','Msg':'消息','People':'联系人','Set':'设置','Map':'地图','Screenshots':'截屏','Quick reply':'快捷回复','Public channel':'公开频道','Private channel':'私有频道','Now playing':'正在播放','Display & language':'显示与语言','Write':'写消息','Recording':'录音','Lock':'锁屏','Wake':'唤醒','Continue':'继续','Find':'发现','Phrase':'短语','Touch test':'触控测试','Add':'新增','Track':'轨迹','Waypoint':'航点','Tile':'离线地图'})
    if names:
        t=re.sub(r'(?<![A-Za-z])('+ '|'.join(re.escape(k) for k in sorted(names,key=len,reverse=True))+r')(?![A-Za-z])',lambda m:names[m[0]],t)
    return t

def product_buttons(rows):
    out=['| 功能键/触发 | 用户操作后的结果 | 使用条件 |','| --- | --- | --- |']
    for c in rows:
        assert len(c)==4,c
        values=[product_text(c[0]),product_text(c[2]),product_text(c[3])]
        out.append('| '+' | '.join(v.replace('|','&#124;') for v in values)+' |')
    return '\n'.join(out)+'\n'


# Product component tables: proportions are design proposals, not measured pixels.
SCENARIOS={
 'Home':'查看时间和未读概况，进入常用功能。','Chats':'先选择公开、私有或联系人会话，再浏览消息。',
 'Contacts':'查找节点、查看状态、收藏常用联系人并发起私聊。','Location':'查看位置、离线地图及载入的轨迹，调整地图视野。',
 'Settings':'按使用经验选择基础或高级配置。','Conversation':'浏览同一频道或联系人的消息并准备回复。',
 'Composer':'使用设备本机键盘编辑和发送消息。','ContactDetail':'查看节点身份，决定添加联系人或开始会话。',
 'Discover':'浏览近期收到广播的节点。','Radio':'选择与使用地区和其他设备一致的通信预设。',
 'Quick':'快速调整显示、语言、时区，或锁屏休眠。','Locked':'不解锁设备，也能简单浏览未读信息。',
 'About':'查看设备与版本信息，查找蓝牙配对码。','SetupName':'首次使用时选择语言并命名设备。',
 'SetupRadio':'首次使用时选择通信地区与预设。','SetupPrivacy':'决定是否启用定位，了解位置分享规则。',
 'SetupFinish':'检查设置，确认后开始使用设备。','Input':'填写名称、联系人公钥或频道资料。',
 'Emoji':'用简短表情补充消息。','Confirm':'执行有影响的操作前，核对内容并选择继续或取消。',
 'MessageDetail':'阅读完整消息正文和投递状态。','Phrases':'用常用短语快速回复，再进入编辑确认。',
 'Diagnostics':'检查设备运行、存储和外设状态。','TouchTest':'检查触摸位置是否准确。',
 'Audio':'选择录音、音乐或返回正在播放的内容。','PublicChannels':'找到默认公开频道或其他公开频道。',
 'PrivateChannels':'选择使用共享密钥的频道。','ChatContacts':'选择聊天联系人并进入私聊。',
 'BasicSettings':'完成日常设置，便于新用户使用。','AdvancedSettings':'进入定位、硬件状态和管理功能。',
 'Recordings':'录制语音，停止保存后选择录音回放。','Music':'选择SD卡上的音乐并开始播放。',
 'NewChannel':'选择公开或私有类型并填写新频道资料。','AudioPlayer':'控制当前音频的播放进度、曲目和音量。',
 'Tracks':'新增轨迹记录，或选择已保存轨迹进行操作。','TrackDetail':'将轨迹载入地图，或删除不再需要的轨迹。',
 'Waypoints':'保存当前位置，或浏览已保存地点。','WaypointDetail':'在地图中查看航点，或删除该航点。',
 'Tiles':'检查离线地图读取方式、开关和缩放。','GPSStatus':'查看定位进度，必要时重启定位。',
 'Apps':'集中进入音频、定位和其他设备工具。','Administration':'进行重启、关机或恢复出厂等管理操作。',
 'Compass':'移动时判断GPS行进方向。','ScreenshotSettings':'开启截屏，保存当前画面用于记录和反馈。',
}

def component_table(key,d):
    assert key in SCENARIOS,key
    buttons=d['buttons'];rows=[];covered=[]
    card='深色卡片；浅色文字；黄绿色重点'
    info='暗色背景；浅色正文；灰色说明'
    green='黄绿色按钮；深色文字'
    red='深色底；红色操作名称'
    roots={'Home','Chats','Contacts','Location','Settings','Audio','Locked','SetupName'}
    def row(name,parts,color,interaction,desc='',indexes=()):
        for n in indexes:
            assert 0<=n<len(buttons),(key,n)
            covered.append(n)
        rows.append((name,parts,color,interaction,desc,list(indexes)))
    toolbar={'Chats':[3],'Contacts':[3,4],'Discover':[1],'PublicChannels':[1],'PrivateChannels':[1],'ChatContacts':[3]}.get(key,[])
    header='显示页面标题、电量百分比和电池图标。'
    if key=='Locked':header='显示未读信息总数、电量百分比和电池图标。'
    if key not in roots:header+=' 点击左上橙色箭头：'+d['back']
    row('状态栏'+('与返回键' if key not in roots else ''),2,'深绿底；黄绿标题；浅灰状态'+('；橙色返回键' if key not in roots else ''),'显示'+('；点击' if toolbar or key not in roots else ''),header,toolbar)
    def nav(parts=3):row('底部菜单提示区',parts,'深青底；浅灰提示；黄绿色标记','左右滑动；底边上滑','显示所在页面和手势提示；左滑下一页，右滑上一页；底边上滑打开六项主导航。')
    def listing(parts,indexes=None,name='内容列表',desc=None):
        row(name,parts,card,'上下滑动；点击',desc if desc is not None else d['body'],range(len(buttons)) if indexes is None else indexes)
    if key=='Home':
        row('大号时钟区',7,'深色卡片；黄绿色数字；浅灰秒数','查看；轻量动效','显示时分、秒数和校时状态；冒号闪动、灯条扫描；未校时时显示横线。')
        listing(8,name='常用功能与快捷入口',desc='单栏浏览消息、联系人、应用和自定义入口，列表末尾保留新增按钮。');nav()
    elif key=='Chats':
        for i,name in enumerate(['公开频道入口','私有频道入口','联系人会话入口']):row(name,5,card,'点击','',[i])
        nav()
    elif key in ['Contacts','ChatContacts']:
        listing(15 if key=='Contacts' else 18,[0,1,2],name='联系人卡片',desc='名称固定显示；近期可见为绿色，否则红色。信号格与电量每1秒交替；点击卡片原位展开位置、SNR、RSSI；星标点亮后置顶。')
        if key=='Contacts':nav()
    elif key=='Location':
        row('地图画布与定位信息',13,'地图图片或暗色栅格；绿色当前位置；琥珀色旧位置','查看；点击定位状态','显示离线地图、当前位置或最后位置、缩放层级和卫星状态；没有位置时不显示虚假定位点。',[4])
        row('地图移动与缩放区',3,card,'点击','保留四向移动、定位回中、放大、缩小按钮。',[0,1,2]);nav(2)
        row('地图菜单与轨迹退出键',None,'深色悬浮按钮；黄绿色图标','点击','悬浮在地图上方，不改变主布局；仅载入轨迹时显示退出按钮。',[3,5])
    elif key=='Settings':listing(15);nav()
    elif key=='Conversation':
        row('会话类型区',2,info,'查看','显示当前会话名称及频道或私聊类型。')
        listing(12,[0],name='消息记录区',desc='收到与发出的消息区分排列，显示发信人、状态和正文预览；上下滑动查看历史。')
        row('回复操作区',4,card+'；写消息为黄绿色','点击','',[1,2])
    elif key in ['Composer','Input']:
        row('输入内容区',4,'暗底；青色边框；浅色文字','查看编辑结果','显示正在编辑的内容，较长时显示末尾，避免挤占键盘。')
        row('长度提示区',1,'灰色说明；超限时强调提示','查看','显示输入长度与当前允许上限。')
        row('键盘与编辑工具',9,card,'点击输入','字母、数字和符号键盘；大小写、空格、删除等工具。',list(range(6 if key=='Composer' else 5)))
        row('完成操作区',4,green,'点击','',list(range(6,len(buttons))) if key=='Composer' else [5])
    elif key=='ContactDetail':
        row('节点身份信息',13,info,'查看','显示节点名称、公钥、是否已保存等信息。')
        row('联系人操作区',5,card,'点击','按钮随联系人是否已保存而变化。',range(len(buttons)))
    elif key=='Discover':listing(18,[0],name='发现设备列表',desc=d['body'])
    elif key in ['Radio','SetupRadio']:
        row('地区选择提示',3,info,'查看','说明需要选择适用地区和通信预设。')
        listing(10,[0],name='地区预设列表',desc='上下浏览并点击选择预设，选中项突出显示。')
        row('选中预设说明',2,'灰色说明；黄绿色重点','查看','显示所选预设的关键通信参数。')
        row('翻页与确认区',3,card+'；确认为黄绿色','点击','',[1,2])
    elif key=='Quick':listing(18,name='快捷操作按钮区')
    elif key=='Locked':
        row('未读信息卡片区',12,'深色圆角卡片；黄绿发信人；浅色摘要','上下滑动','最多一次显示2条未读；上下滑动查看最新10条；每条包含发信人和摘要，当前摘要最多15个字符。',[1])
        row('浏览与解锁提示',2,'灰色说明文字','查看','提示下滑查看更多未读及向右滑动解锁。')
        row('解锁栏',4,'深青轨道；黄绿色滑块；浅色提示','滑块向右拖动并松手','从轨道左端拖到右端后解锁；不再使用长按。',[0])
    elif key=='About':row('设备信息区',18,info,'查看',d['body'])
    elif key=='SetupName':
        row('欢迎说明区',6,info,'查看','介绍设备并提示开始设置。')
        row('设备名称区',5,card,'点击','',[0]);row('语言选择区',3,card+'；已选项黄绿色','点击','',[1]);row('下一步',4,green,'点击','',[2])
    elif key=='SetupPrivacy':
        row('定位说明区',5,info,'查看','说明是否定位由用户选择。');row('定位开关区',5,card,'点击','',[0]);row('隐私说明区',4,info,'查看','自动分享位置默认关闭，无需定位也能聊天。');row('继续',4,green,'点击','',[1])
    elif key=='SetupFinish':
        row('设置摘要区',14,info,'查看','展示设备名称、所选地区预设和位置分享规则。');row('保存并开始',4,green,'点击','',[0])
    elif key in ['Emoji','Phrases']:listing(18,name='可选内容列表')
    elif key=='Confirm':
        row('确认内容区',14,info,'查看','展示本次操作及其影响，避免误操作。');row('取消与确认区',4,card+'；当前取消为黄绿色','点击','',[0,1])
    elif key=='MessageDetail':
        row('发信人与状态区',3,info,'查看','显示消息来源和投递状态。');row('完整正文区',12,info,'上下滑动','浏览完整正文；消息已被淘汰时显示提示。');row('翻阅操作区',3,card,'点击','',[0])
    elif key=='Diagnostics':
        row('硬件与存储状态区',14,info,'查看',d['body']);row('测试入口区',4,card,'点击','',[0])
    elif key=='TouchTest':
        row('测试说明区',3,info,'查看','提示依次触摸四个角。');row('触控测试画布',15,'暗色底；黄绿色目标框和触摸标记','点击四角；查看反馈','显示触摸位置和坐标，用于判断是否准确。')
    elif key=='Audio':listing(15,name='音频功能入口');nav()
    elif key in ['PublicChannels','PrivateChannels']:listing(18,[0],name='频道列表')
    elif key in ['BasicSettings','AdvancedSettings','Apps']:listing(18,name='设置列表' if key!='Apps' else '应用入口列表')
    elif key=='Recordings':
        row('录制状态与时长',3,'灰色路径；录制时黄绿色时长与电平','查看','空闲时显示目录，录制时显示录音时长和声音电平。')
        listing(11,[2],name='录音文件列表',desc='最新完成录音置顶并标注最新；点击文件进入播放器。')
        row('录音控制栏',4,green,'点击一次开始；再次点击停止','同一按钮在开始录音与波形加停止提示之间切换，不另设停止按钮。',[0,1])
    elif key=='Music':
        row('目录与播放状态',2,info,'查看','显示音乐目录或当前播放状态。');listing(13,[0],name='音乐文件列表');row('停止操作区',3,card,'点击','',[1])
    elif key=='NewChannel':listing(18,name='频道类型选择区')
    elif key=='AudioPlayer':
        row('曲目信息与状态',4,info,'查看','显示文件名和播放、暂停、停止或错误状态。');row('声音波形区',3,'深色卡片；黄绿色波形','查看','展示声音波形，不使用虚构频谱。')
        row('时长与进度区',3,'深青进度轨；黄绿色进度','点击；拖动','显示已播和总时长，调整播放位置。',[3]);row('播放控制栏',4,card+'；播放暂停为黄绿色','点击','',[0,1,2]);row('音量控制栏',4,'黄绿色滑杆；浅色百分比','点击；拖动','',[4,5])
    elif key=='Tracks':
        row('轨迹记录控制区',4,green,'点击','新增轨迹与停止记录使用同一个入口。',[0,1]);listing(14,[2],name='已保存轨迹列表')
    elif key in ['TrackDetail','WaypointDetail']:
        row('选中项目名称',4,info,'查看','显示当前选中的轨迹或航点。');row('查看与删除操作区',14,card+'；删除名称红色','点击','删除前需要二次确认。',[0,1])
    elif key=='Waypoints':
        row('保存当前位置区',4,green,'点击','没有有效定位时改为打开定位状态入口。',[0,1]);listing(14,[2],name='已保存航点列表')
    elif key=='Tiles':
        row('离线地图开关',4,card,'点击','',[0]);row('地图目录说明',6,info,'查看','说明从SD卡/map目录读取地图，并显示当前需要的文件位置。');row('缩放操作区',4,card,'点击','',[1]);row('重新读取区',4,card,'点击','',[2])
    elif key=='GPSStatus':
        row('定位开关与重启',4,card,'点击','',[0,1]);row('定位状态区',9,info,'查看','显示等待数据、搜星、定位成功、卫星数量和数据接收状态。');row('最近位置区',2,info,'查看','有位置时显示坐标，无位置时显示等待提示。');row('打开地图',3,card,'点击','',[2])
    elif key=='Administration':
        for i,name in enumerate(['恢复出厂设置','重启设备','关机']):row(name,6,red,'点击后进入二次确认','',[i])
    elif key=='Compass':
        row('罗盘与行进信息区',14,'暗色底；浅灰刻度；黄绿与红色指针','查看','显示方向盘、行进角度和速度；静止或无有效方向时显示未知。');row('定位入口与限制提示',4,card,'点击；查看说明','说明依赖GPS行进方向，不能测静止朝向。',[0,1])
    elif key=='ScreenshotSettings':
        row('截屏开关',4,card,'点击','',[0]);row('操作说明区',8,info,'查看','长按左上角约1.2秒；移动取消；短按返回仍有效。');row('保存目录与结果说明',6,info,'查看','图片保存到SD卡截屏目录，成功提示需点击关闭。');row('全局截屏触发区',None,'沿用当前页面；成功提示为深青色','左上角长按','该触发区叠加在页面左上方，不额外占用布局。',[1])
    else:raise AssertionError(('Missing component profile',key))
    assert sum(r[1] for r in rows if r[1] is not None)==20,(key,rows)
    assert sorted(covered)==list(range(len(buttons))),('Button coverage',key,covered,len(buttons))
    out=['| 页面组件 | 占比 | 色彩 | 交互方式 | 功能 |','| --- | --- | --- | --- | --- |']
    for name,parts,color,interaction,desc,indexes in rows:
        pieces=[desc] if desc else []
        for n in indexes:
            c=buttons[n];text='• **'+c[0]+'**：'+c[2]
            if c[3] not in ['通常可用','常态可用']:text+='（'+c[3]+'）'
            pieces.append(text)
        values=[name,str(parts)+'/20' if parts is not None else '悬浮叠加',color,interaction,'<br>'.join(pieces)]
        out.append('| '+' | '.join(product_text(v).replace('|','&#124;') for v in values)+' |')
    return '\n'.join(out)+'\n'

def generate():
    ASSETS.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'examples/companion_radio/ui-seeed/UITask.h').read_text(encoding='utf-8')
    pages=[v.strip() for v in re.search(r'enum Page \{(.*?)\};',source,re.S).group(1).split(',')]
    assert len(pages)==44 and set(P)==set(pages),'Update page coverage before publishing'
    # Keep the published discussion at the level of visible behavior.
    overrides={
      'Home':dict(body='设备名称与电量；大号数码时钟、秒数、闪动冒号和扫描灯条；单栏消息、联系人、应用、自定义快捷入口；列表底部新增按钮。',state='未校时时显示横线；完成校时后显示时间与秒数。冒号闪动、灯条扫描；上下浏览可找到应用入口和新增快捷入口按钮。'),
      'Chats':dict(body='公开频道、私有频道、聊天联系人三个入口；顶部新增频道按钮。'),
      'Contacts':dict(state='点击卡片后原位变宽并展开GPS、SNR、RSSI，再次点击收起。15分钟内收到过节点信息时名称为绿色，否则红色；这是近期可见状态。电量依赖对方上报，缺失或超过1小时显示未知。展开时可请求一次节点状态，不会每秒轮询对方。'),
      'Conversation':dict(state='进入会话会标记对应消息已读，并显示最新消息。所有会话合计保留最近48条历史；不同会话内容分开显示。'),
      'Composer':dict(body='会话标题；编辑中的消息；已输入长度与可发送上限；字母/数字符号键盘；大小写、空格、删除、表情、短语、发送。',state='私聊正文最多160字节；频道消息需要预留发信人名称所占长度。界面可切换中英文，目前键盘不支持中文拼音。提交失败保留草稿，成功后返回会话。'),
      'ContactDetail':dict(state='联系人主页以卡片原位展开为主；本页用于发现设备后查看节点身份与添加联系人。'),
      'Radio':dict(state='选择预设后，点击确认才应用设置。当前按地区预设选择，不能自由编辑全部无线电参数；默认发送功率14dBm。'),
      'Quick':dict(body='亮度增加、亮度降低、语言切换、锁屏、时区与休眠六个按钮。',state='使用统一左上返回键退出。'),
      'About':dict(back='返回基础设置。当前即使从应用列表进入，也返回基础设置；是否统一原路返回，列入讨论项。',state='蓝牙开启时显示配对码。'),
      'SetupName':dict(entry='首次使用，或更新后需要重新确认设置时',state='完成引导前不发送消息。重新进入引导不清空原有联系人、消息或SD文件。'),
      'SetupRadio':dict(state='这一步确认地区选择；最后点击保存并开始后，才正式应用。'),
      'SetupFinish':dict(state='保存成功后进入首页，并按用户选择开启GPS；失败时提示原因，保留待确认状态。'),
      'Input':dict(state='内容不符合要求时给出错误提示并保留输入；长度、公钥、共享密钥、重复名称等要求见对应输入场景。'),
      'Emoji':dict(body='微笑、确认、等待、位置、警告五个名称项；点击后插入对应表情。',state='当前为名称列表。'),
      'Confirm':dict(state='确认页面占满屏幕，暂时不能打开主导航。取消按钮为绿色，确认按钮使用普通按钮颜色；该主次关系列入产品讨论。'),
      'PublicChannels':dict(state='包括默认公开频道和用户添加的公开频道；最多可保存40个频道。'),
      'Diagnostics':dict(body='外设工作状态、可用内存、内部存储用量、消息记录状态及SD卡状态。'),
      'Audio':dict(body='录音、音乐两个入口；选择过音轨后增加正在播放入口。'),
      'BasicSettings':dict(body='按顺序：LoRa配置、显示与语言、蓝牙、设备名称、提示音、音量增加、音量降低、关于设备；上下浏览。'),
      'Recordings':dict(state='单次最长30分钟，录音列表最多显示128项。最新完成录音置顶；录制期间不能点播其他文件。保存后可直接进入播放器回放。录音与停止使用同一个按钮。'),
      'NewChannel':dict(state='创建成功后返回对应频道列表，再点击频道进入会话。名称或密钥重复、格式不正确或已满时提示，不覆盖现有频道。'),
      'AudioPlayer':dict(state='可暂停、停止、切换上一首/下一首、拖动进度和音量；当前文件列表首尾循环。MP3跳转和部分文件时长为估算，不承诺精确到秒；接近静音时给出提示。',body='当前文件名、播放状态或错误提示；声音波形；已播放/总时长；进度条；上一首、播放/暂停、下一首、停止；音量滑杆、百分比及加减按钮。'),
      'Tracks':dict(state='文件保存在SD卡的轨迹目录。每5秒记录一次新的有效位置；没有定位时等待，不写入虚假位置。列表最多显示32条。轨迹记录与声音录制分别控制。'),
      'TrackDetail':dict(state='单个轨迹文件不超过4MB；地图展示轨迹概览，保留原始文件。没有有效定位点时提示无法载入；正在记录的轨迹不能删除。'),
      'Waypoints':dict(state='最多保存24个航点；默认自动命名。当前不支持修改名称或手动输入经纬度。'),
      'Tiles':dict(state='从SD卡的/map目录读取地图；无地图时显示栅格。支持缩放3～18级的标准图片瓦片，当前不提供在线下载或其他地图文件格式导入。旧目录文件需由用户自行整理。'),
      'GPSStatus':dict(body='GPS开关、重启；关闭、等待数据、搜星或定位成功等状态；可见及使用卫星数、接收数据统计、最近坐标；打开地图按钮。',state='无位置时显示等待定位；已取得过位置时保留最近坐标，地图区分当前位置与过期位置。'),
      'Apps':dict(body='按顺序为：音频、GPS状态、轨迹、航点、离线地图、快捷操作、关于设备、指南针、截屏；上下滑动浏览。',state='这是首页内的应用列表，通过上下滑动浏览；不增加横向主页面数量。'),
      'Administration':dict(state='关机为低功耗休眠，可使用BOOT键唤醒；恢复出厂会清除内部身份、联系人和设置，保留SD文件。'),
      'Compass':dict(state='使用GPS行进方向，需要有效定位并移动约1.9km/h以上。静止或方向过期时显示未知并提示移动。设备没有磁力计，因此不显示静止时设备朝向。'),
      'ScreenshotSettings':dict(body='截屏开关；长按屏幕左上角的操作说明；保存目录；需要SD卡及点击关闭提示的说明。',state='默认关闭；开启后长按屏幕左上角约1.2秒，一次按下只截一张；手指移动取消。成功后显示文件位置，点击关闭提示。SD不可用或保存失败时显示错误，不显示成功。'),
    }
    for k,updates in overrides.items():P[k].update(updates)
    visible_labels={'Messages':'消息','Contacts':'联系人','Apps':'应用','Public channel':'公开频道','Private channel':'私有频道','顶部Add':'顶部新增','顶部Find':'顶部发现','自定义入口':'自定义快捷入口','Recording':'录音','Music':'音乐','Now playing':'正在播放','Sound':'提示音','Device name':'设备名称','Display & language':'显示与语言','Hardware status':'硬件状态','Bluetooth':'蓝牙','Screenshots on/off':'截屏开关','全局左上长按':'屏幕左上角长按','Record':'开始录音','Stop':'停止','Shortcuts':'快捷入口','GPS on/off':'GPS开关','Turn on/off':'GPS开关','Restart GPS':'重启GPS','Open map':'打开地图','Write':'写消息','Quick reply':'快捷回复','Send':'发送','Emoji':'表情','Phrase':'短语','Cancel':'取消','Confirm':'确认','Save & start':'保存并开始','Next':'下一步','Continue':'继续','Confirm band':'确认预设','预设项':'地区预设','Input':'输入','Factory reset':'恢复出厂设置','Reboot':'重启','Shut down':'关机','Sleep':'休眠','Volume + / -':'音量增加/降低','Brightness + / -':'亮度增加/降低','Language':'语言切换','Lock':'锁屏','Load on map':'载入地图','Delete track':'删除轨迹','View on map':'地图查看','Delete waypoint':'删除航点','Exit track':'退出轨迹显示','GPS状态条':'定位状态条'}
    for d in P.values():
        for c in d['buttons']:c[0]=visible_labels.get(c[0],c[0])
        for f in ['entry','body','back','state']:d[f]=product_text(d[f])
        d['buttons']=[[product_text(c[0]),c[1],product_text(c[2]),product_text(c[3]).replace('常态可用','通常可用')] for c in d['buttons']]
    # Correct wording that only existed to explain implementation details.
    P['Home']['buttons'][-1][2]='打开快捷入口选择窗'
    P['Location']['buttons'][3][2]='打开或关闭地图菜单'
    P['Chats']['buttons'][2][0]='联系人'
    P['ScreenshotSettings']['buttons'][-1][2]='保存当前画面，成功后显示文件位置提示'
    P['Composer']['buttons'][0][2]='追加选中的字符'
    P['Input']['buttons'][-1][2]='检查输入内容，进入下一步或返回输入前的页面'
    P['Input']['buttons'][-1][3]='内容非空且符合当前填写项要求'
    P['SetupFinish']['buttons'][0][2]='保存设置并进入首页'
    P['SetupFinish']['buttons'][0][3]='地区已选择，设置保存成功'
    P['Radio']['buttons'][-1][3]='已选择预设，设备当前可以应用设置'
    P['Radio']['buttons'][-1][2]='应用设置并进入首页'
    P['ScreenshotSettings']['buttons'][-1][3]='已开启截屏，手指在屏幕左上角保持约1.2秒且不移动'
    P['Confirm']['buttons'][-1][2]='执行本次确认的操作，结果见二次确认表'
    P['Contacts']['buttons'][0][2]='原位展开或收起；同一时间只展开一个节点'
    def visible_svg_text(self,x,y,t,*args,**kwargs):return original_svg_text(self,x,y,product_text(t),*args,**kwargs)
    original_svg_text=Svg.text
    Svg.text=visible_svg_text
    roots={'Home','Chats','Contacts','Location','Settings','Audio','Locked','SetupName'}
    doc=['''# Wio Tracker L2 Pro UI 产品讨论稿

> 当前版本：0.5.1；更新日期：2026-11-12。
>
> 本文用于产品团队讨论页面内容、功能入口、操作流程和交互层级。页面描述以当前可见行为为基线；尚未统一的体验单独列为讨论项。
>
> 图片分为**实机截图**和**页面结构示意图**。示意图用于解释布局，不代表所有数据状态都已实测。

## 1. 产品范围

- 320×240横向触摸屏，中英文界面。
- 6个主页面：**首页 → 消息 → 联系人 → 地图 → 音频 → 设置**，循环切换。
- 共44个页面，覆盖首次使用引导、会话、联系人、地图、音频和设备工具。
- [全部页面图集](l2-ui-gallery.html)可在浏览器查看或打印。

## 2. 实机图与当前反馈

### 首页

![首页实机截图](assets/l2-ui-0.5.1/device-home.png)

### 锁屏

![锁屏实机截图](assets/l2-ui-0.5.1/device-current.png)

首页和锁屏为设备真实画面；其余页面配结构示意图。示意图中的时间、节点、消息、位置和文件名使用占位说明。

| 功能 | 当前反馈 |
| --- | --- |
| 左滑切下一页、首页大时钟与动效 | 用户确认正常 |
| 截屏开关、长按触发、成功提示与点击关闭 | 用户确认正常 |
| GPS定位、录音回放与音量 | 前一版本已确认，仍需随版本持续复测 |
| 全页面显示、多节点状态、地图文件、轨迹与航点、长时间使用 | 待专项验证 |

## 3. 全局交互与层级

### 3.1 布局与视觉

| 区域 | 产品要求 |
| --- | --- |
| 顶部状态栏 | 深绿色，左侧页面标题，右侧电量百分比和电池图标；需要确认通信设置时显示提示 |
| 主体内容 | 单列圆角卡片，内容较多时上下浏览；长内容不挤占固定操作区 |
| 底部提示 | 主页面显示当前页位置、左滑下一页和上滑菜单提示 |
| 底部导航 | 从屏幕底边上滑出现；颜色与正文和顶栏区分，体积紧凑 |
| 返回键 | 子页面统一左上角，橙色、仅箭头图标，与其他操作颜色区分 |
| 字体 | 正文清楚易读，标题和按钮加粗；首页使用大号数码时钟 |
| 颜色 | 暗色背景、较亮卡片、黄绿色强调；红色提示管理操作或节点未近期可见 |
| 动效 | 首页闪动冒号和扫描灯条；操作不出现整屏闪黑 |
| 电量 | 显示百分比与电池；未知显示占位，低电量突出提示。当前为估算，后续可调整曲线 |

### 3.2 操作规则

| 操作 | 结果 |
| --- | --- |
| 主页面左滑 | 下一页，末页回首页 |
| 主页面右滑 | 上一页，首页回末页 |
| 子页面左右滑 | 不自动切到其他主页面 |
| 内容上下滑 | 浏览本页列表或正文，滑动不误触按钮 |
| 点击 | 选择卡片或执行按钮功能 |
| 底边上滑 | 打开主导航；引导、锁屏和二次确认时不可打开 |
| 导航下滑、点空白或选中项 | 收起导航，不触发下方内容 |
| 导航5秒无操作 | 自动收起；手指按住期间不隐藏 |
| 播放器滑杆 | 调整进度或音量，不触发切页 |
| 锁屏滑块向右 | 从左端拖到右端并松手解锁，返回锁定前页面 |
| 锁屏卡片区下滑 | 查看较旧未读；上滑查看较新未读，每屏两条 |
| 左上角长按约1.2秒 | 开启截屏后保存当前画面；移动取消，短按返回仍有效 |
| 30秒无操作 | 熄屏；首次触摸仅唤醒 |
| 60秒无操作 | 进入锁屏 |
| BOOT键 | 正常使用时短按打开或关闭导航，长按返回；休眠后可唤醒 |
| 侧边唤醒键 | 亮屏时锁屏熄屏；灭屏时点亮 |

### 3.3 覆盖与返回

![页面覆盖层级](assets/l2-ui-0.5.1/layers.svg)

从下到上依次为：**页面内容 → 返回键 → 局部菜单/选择窗 → 底部主导航 → 状态或错误提示**。

- 浮窗打开后，点击窗外只关闭浮窗，不触发底层按钮。
- 底部导航出现时优先接收操作；状态、错误、截屏提示显示在导航之上。
- 二次确认和锁屏占满屏幕，暂时限制其他入口。
- 子页面一般返回进入前的页面；当前部分页面有固定返回目标，下文明确标注，需讨论是否统一。

### 3.4 核心流程

```mermaid
flowchart TD
  引导名称语言 --> 引导地区预设 --> 引导定位隐私 --> 引导确认 --> 首页
  首页 --> 消息
  首页 --> 联系人
  首页 --> 应用列表
  消息 --> 公开频道列表 --> 会话
  消息 --> 私有频道列表 --> 会话
  消息 --> 聊天联系人 --> 会话
  消息 --> 新增频道 --> 填写频道资料 --> 对应频道列表
  联系人 --> 发现设备 --> 节点详情
  会话 --> 消息编辑
  会话 --> 消息详情
  会话 --> 快捷短语 --> 消息编辑
  应用列表 --> 录音与音乐
  应用列表 --> GPS状态
  应用列表 --> 行进罗盘
  应用列表 --> 截屏设置
  地图 --> 地图菜单 --> 轨迹管理 --> 轨迹操作 --> 地图
  地图菜单 --> 航点列表 --> 航点操作 --> 地图
  地图菜单 --> 离线地图设置
  设置 --> 基础设置
  设置 --> 高级设置 --> 管理操作 --> 二次确认
```

## 4. 页面目录
''','| 编号 | 页面 | 入口 |','| --- | --- | --- |']
    for i,k in enumerate(pages):doc.append(f'| {i+1:02d} | [{P[k]["title"]}](#ui-{i:02d}) | {P[k]["entry"]} |')
    doc.append('\n## 5. 逐页内容与操作\n\n逐页按 **页面组件、占比、色彩、交互方式、功能** 五列描述。**占比按整屏高度20等份给出产品讨论建议，不是当前界面的实测值**；每页独占区域合计20/20，悬浮组件单独标为叠加，不重复计入。色彩参考现有界面，均可在产品讨论中调整。\n\n示意图用于理解当前布局，未按建议占比重新设计；表格中的占比、图片与最终视觉稿需在定稿时统一。所有页面仍遵循通用交互规则。\n')
    for i,k in enumerate(pages):
        d=P[k];wire(k,d,k in roots)
        doc += [f'<a id="ui-{i:02d}"></a>\n\n### {i+1:02d} {d["title"]}\n',f'![{d["title"]}结构示意图](assets/l2-ui-0.5.1/{k}.svg)\n',f'**入口：** {d["entry"]}。\n',f'**使用场景：** {SCENARIOS[k]}\n',component_table(k,d),f'**返回：** {d["back"]}\n']
        if d['state']:doc.append(f'**补充规则：** {d["state"]}\n')
    doc.append('''
## 6. 填写信息的六种场景

各场景共用输入键盘，根据任务显示不同标题、校验要求和完成目标。

| 填写项 | 要求 | 完成后 |
| --- | --- | --- |
| 设备名称 | 不超过31字节 | 返回编辑前页面；引导期间最后统一保存 |
| 联系人公钥 | 完整64位十六进制，不得重复 | 继续填写联系人名称 |
| 联系人名称 | 不超过31字节 | 保存联系人，回联系人主页 |
| 公开频道名 | 以#开头，后面不能为空，不超过31字节 | 保存成功回公开频道列表 |
| 私有频道名 | 不超过31字节 | 继续填写共享密钥 |
| 私有频道共享密钥 | 完整32位十六进制 | 保存成功回私有频道列表 |

目前长度按存储字节计数，不等同所有语言的可输入字数。是否改用更直观的字数提示，列入讨论项。
''')
    for k,title,_,_ in INPUTS:
        d=dict(P['Input']);d['title']=title;wire('Input-'+k,d)
        doc.append(f'\n### {title}\n\n![{title}填写示意图](assets/l2-ui-0.5.1/Input-{k}.svg)\n'+component_table('Input',d))
    doc.append('''
公开频道：选择公开类型 → 输入名称 → 保存 → 公开频道列表 → 点击频道进入会话。

私有频道：选择私有类型 → 输入名称 → 输入密钥 → 保存 → 私有频道列表。填写密钥时返回，当前会退出到新增频道类型选择。

手工联系人：联系人主页点击添加 → 输入公钥 → 输入名称 → 保存并返回联系人主页。

## 7. 菜单、选择窗与提示
''')
    for key,title,entry,body,raw in OVERLAYS:
        buttons=[]
        for line in raw.splitlines():
            c=line.split('~');buttons.append(c+['常态可用'] if len(c)==3 else c)
        body=re.sub(r'300×58，x10/y182；|304×197，|x174/y73/139×124；','',body)
        if key=='OV-01':body='六个入口：首页、消息、联系人、地图、音频、设置。体积紧凑，显示在内容和局部菜单之上。'
        overlay_image((key,title,product_text(entry),product_text(body),raw))
        doc += [f'\n### {title}\n',f'![{title}结构示意图](assets/l2-ui-0.5.1/{key}.svg)\n',f'**入口：** {product_text(entry)}。\n\n**显示与交互：** {product_text(body)}\n',product_buttons(buttons)]
    lay=Svg(310)
    for i,(t,color) in enumerate([('5 状态 / 错误 / 截屏提示','#6d3920'),('4 底部主导航',TEAL),('3 快捷入口选择窗 / 地图菜单',TEAL),('2 统一返回键','#8a5a00'),('1 页面背景 / 状态栏 / 内容',CARD)]):
        y=12+i*51;lay.rect(12,y,296,42,color);lay.text(22,y+26,t,WHITE,13)
    lay.text(12,282,'上层优先响应，点击不穿透到底层',MUTED,11);lay.save('layers.svg')
    doc.append('''
### 快捷入口候选

最多可添加8个。当前候选为：地图、音频、轨迹、航点、GPS状态、LoRa配置、快捷操作、关于设备、音乐、新增频道、硬件状态。指南针和截屏目前从应用列表进入，是否加入候选可讨论。

### 二次确认后的结果

| 操作 | 取消 | 确认 |
| --- | --- | --- |
| 删除轨迹 | 返回轨迹操作页 | 删除成功回轨迹列表；正在记录的轨迹不可删 |
| 删除航点 | 返回航点操作页 | 删除成功回航点列表 |
| 关闭蓝牙 | 返回基础设置 | 断开连接并关闭蓝牙 |
| 恢复出厂 | 返回管理操作 | 清内部身份、联系人、设置并重启；保留SD文件 |
| 重启 | 返回管理操作 | 保存未完成内容、结束录制任务后重启 |
| 关机/休眠 | 返回原页面 | 保存未完成内容、结束任务并休眠，BOOT唤醒 |

## 8. 内容保存与文件位置

| 内容 | 用户可见规则 |
| --- | --- |
| 设置 | 名称、语言、时区、背光、定位与通信设置重启后保留；更新后可能需要重新确认通信预设 |
| 联系人 | 收藏保存并排在前面；不同联系人会话独立 |
| 频道 | 默认公开频道保留；最多40个频道 |
| 消息历史 | 所有会话合计最近48条；锁屏最多展示最新10条未读，每屏2条 |
| 草稿 | 最多保存12个会话的草稿；保存失败提示并保留输入 |
| 音量 | 0～100%，重启保留；加减按钮每次10%，也可拖动调整 |
| 地图 | SD卡/map目录，按/map/层级/x/y.png组织；缺失时显示栅格 |
| 轨迹 | SD卡/tracks目录；有有效定位才记录；载入、退出载入与删除分别控制 |
| 航点 | 保存在SD卡，最多24个；需有效定位才新增 |
| 录音 | SD卡/record目录，WAV格式；最长30分钟，最新录音置顶 |
| 音乐 | SD卡/music目录，MP3格式；列表最多128项 |
| 截屏 | SD卡/screenshots目录，BMP图片，320×240；不覆盖原图片 |
| SD异常 | 显示失败原因，不自动清空或格式化卡 |

开机、关机、收发消息和确认反馈有提示音，可关闭。录音或播放期间不插入消息提示音，消息内容仍更新。直接断电无法播放关机音。

消息会按过程显示已接收、排队中、已发出、对方已确认、未获确认或失败。没有确认不能显示为对方已收到；发送失败保留草稿。

## 9. 产品讨论事项

以下均为待讨论项，不代表已经改动当前交互。

| 议题 | 当前体验 | 讨论方向 |
| --- | --- | --- |
| 返回一致性 | 关于设备固定回基础设置；新增频道固定回消息首页；短语取消回编辑页 | 是否统一返回进入前的页面，并保留浏览位置 |
| 快捷操作返回 | 连续跨页使用后返回目的地不一定符合预期 | 统一返回规则 |
| 消息联系人页的+ | 当前进入新增频道；添加联系人在联系人主页 | 是否让+的含义随页面内容变化 |
| 二次确认主次 | 取消为绿色，确认为普通颜色 | 是否按操作风险统一按钮颜色和主次 |
| 文字长度 | 当前显示字节数，部分长文案或文件名会截断 | 是否改为更直观的长度和完整查看方式 |
| 默认提示音 | 录制/播放期间消息不插音 | 是否需要额外可见提示方式 |
| 首页入口 | 可自定义8项，指南针/截屏尚不在候选内 | 是否开放全部应用或允许排序 |
| 罗盘命名 | 依赖移动中的GPS方向，静止不测朝向 | 是否统一命名为“行进罗盘”并突出限制 |
| 锁屏摘要长度 | 当前最多15个字符；参考样式中为20个字符 | 是否调整为20个字符，及其在两张卡片中的可读性 |
| 页面占比 | 本文以20等份提出讨论建议，尚未按此重做视觉 | 确认各区域比例，再统一视觉稿、示意图和最终界面 |
| 消息历史容量 | 全机48条，旧消息可能被淘汰 | 是否扩大容量或提供用户可控的历史管理 |
| 地图导入 | 需要用户按指定目录准备图片文件 | 是否增加导入指引或地图包管理 |

## 10. 产品验证清单

| 类别 | 检查内容 |
| --- | --- |
| 页面导航 | 左右滑方向与循环、子页返回、底部导航不点穿及5秒隐藏 |
| 首页与显示 | 时钟未校时/校时/时区、动效无闪黑、入口可达、中英文清晰 |
| 消息与联系人 | 公开/私有/私聊入口、添加、收藏、展开、状态过期、草稿与发送失败 |
| 地图 | 无图栅格、真实地图、移动缩放、定位过期、轨迹记录/加载/退出/删除、航点操作 |
| 音频 | 录制时长和单按钮停止、最新置顶、回放、暂停/停止、切曲、进度与音量 |
| 截屏 | 关时不触发、长按一次一张、短按返回、移动取消、无卡失败、成功弹窗可关闭 |
| 定位与罗盘 | 无定位/搜星/定位成功、移动/静止、方向过期时不显示误导数据 |
| 设置与管理 | 设置重启保留、蓝牙连接、确认取消、关机唤醒、恢复出厂影响清楚 |

## 11. 图片说明

共59张图：2张实机截图、44张页面结构图、6张填写信息示意图、6张菜单/提示图，以及1张层级图。实机图与示意图均明确区分，可离线浏览。
''')
    cards=[]
    figures=[('device-home.png','首页 · 实机截图'),('device-current.png','锁屏 · 实机截图')]+[(k+'.svg',P[k]['title']+' · 页面示意图') for k in pages]+[('Input-'+k+'.svg',title+' · 填写示意图') for k,title,_,_ in INPUTS]+[(o[0]+'.svg',o[1]+' · 交互示意图') for o in OVERLAYS]+[('layers.svg','页面层级 · 示意图')]
    for file,title in figures:cards.append('<figure><figcaption>'+esc(title)+'</figcaption><img src="assets/l2-ui-0.5.1/'+esc(file)+'" alt="'+esc(title)+'" width="320"></figure>')
    gallery='<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>L2 UI产品讨论图集</title><style>body{margin:24px;background:#0b1214;color:#f4f7f6;font:16px system-ui,Microsoft YaHei}a{color:#beed35}main{display:grid;grid-template-columns:repeat(auto-fit,340px);gap:18px}figure{margin:0;padding:8px;border:1px solid #35515a;border-radius:10px;break-inside:avoid}figcaption{padding:4px 2px 12px}img{display:block;max-width:100%}@media print{body{background:white;color:black}figure{break-inside:avoid}}</style><h1>L2 UI 0.5.1 产品讨论图集</h1><p>2张实机截图，其余为页面示意图，用于讨论布局与交互。</p><p><a href="l2-ui-requirements.md">打开UI产品讨论文档</a></p><main>'+''.join(cards)+'</main></html>'
    (ROOT/'docs/l2-ui-gallery.html').write_text(gallery,encoding='utf-8')
    out=ROOT/'docs/l2-ui-requirements.md';out.write_text('\n'.join(doc),encoding='utf-8')
    for svg in ASSETS.glob('*.svg'):
        text=svg.read_text(encoding='utf-8').replace('源码结构图','页面示意图').replace('源码结构','页面结构').replace('源码','页面')
        svg.write_text(text,encoding='utf-8')
    manifest=dict(version='0.5.1',audience='product_discussion',date='2026-11-12',pages=44,images=[])
    for file,title in figures:
        q=ASSETS/file;manifest['images'].append(dict(file=file,title=title,type='device_screenshot' if q.suffix=='.png' else 'page_diagram',sha256=hashlib.sha256(q.read_bytes()).hexdigest()))
    (ASSETS/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Generated product-facing specification: 44 pages, 59 images; no code links or action identifiers')
if __name__=='__main__':generate()
