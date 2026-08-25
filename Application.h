#pragma once

#include<SFML/Graphics.hpp>
#include<SFML/Audio.hpp>
#include<vector>

class Application
{
//系统参量
private:
	sf::RenderWindow window_;    //窗口

	bool isRestart = false;    //死亡重开标记
	bool isPaused_ = false;    //暂停参量

	bool JumpRequested_ = false; //跳跃键标记
	float velocity_Y = 0.0f;    //垂直速度
	bool isOnGround_ = false;    //是否在地面上的标记

	sf::Clock clock_;    //增量时间
	float deltaTime_ = 0.f;    //时间差

	bool isTrapTouch = false;    //陷阱触发标记
	int playerState_ = 0;    //角色状态标记
	int pickupState_ = 0;    //拾取物状态标记
	float fadeTime_ = 0.f;    //淡出时间
	float playerFadeTime_ = 0.0f;    //角色淡出时间

	int current_music_ = 1;    //背景音乐循环标记
	int Easter_egg_ = 1;    //彩蛋标记

//角色参量
private:
	//纹理//
	sf::Texture T_player_;    //主角
	sf::Texture T_player_glance_;    //主角惊鸿一瞥
	sf::Texture T_player_big_laugh_;    //主角捧腹大笑
	sf::Texture T_collection_1;    //拾取物状态1
	sf::Texture T_collection_2;    //拾取物状态2
	sf::Texture T_Spine_;    //陷阱尖刺
	sf::Texture T_Deco_tree_;    //装饰物:树
	sf::Texture T_Deco_Leave_Left_;    //装饰物:左叶片
	sf::Texture T_Deco_Leave_Right_;    //装饰物:右叶片
	sf::Texture T_Deco_cloud_mass_;    //装饰物:云朵
	sf::Texture T_BackGround_1;    //背景1
	sf::Texture T_BackGround_2;    //背景3
	sf::Texture T_BackGround_3;    //背景2
	sf::Texture T_S_Ground_1;    //冬天地面1
	sf::Texture T_S_Ground_2;    //冬天地面2
	sf::Texture T_S_Ground_3;    //冬天地面3

	//形状//
	sf::RectangleShape player_;    //主角形状
	sf::RectangleShape collection_;    //拾取物形状
	std::vector<sf::RectangleShape> Trap_;    //陷阱形状

	//背景//
	sf::RectangleShape BackGround_;    //背景形状
	std::vector<sf::RectangleShape> Grounds_;    //地形形状

	//音频//
	sf::SoundBuffer B_Collection_;    //固定collection缓存区
	sf::SoundBuffer B_Die_;    //固定die缓存区
	sf::SoundBuffer B_Die_1;    //奶龙笑缓存区
	sf::SoundBuffer B_Jump_;   //跳跃音效缓存区
	//长音频
	sf::Music M_Background_1;    //第一关戴夫
	sf::Music M_Background_2;    //第一关哈吉米
	sf::Music M_Background_3;
	sf::Music M_Background_4;
	sf::Music M_Collection_1;    //牛肉面
	sf::Music M_Collection_2;    //加麻
	sf::Music M_Die_3;    //急急如律令
	sf::Music M_Hurdle2_1;    //恋爱循环安迪
	sf::Music M_Hurdle2_2;    //猪猪侠安迪

	sf::Sound S_Collection_;    //固定collection
	sf::Sound S_Die_;    //固定die
	sf::Sound S_Die_1;    //奶龙笑
	sf::Sound S_Jump_;    //跳跃音效
//私有接口
private:
	//事件接口
	void HandleEvent(const sf::Event::Closed);    //关闭窗口
	void HandleEvent(const sf::Event::Resized& resized);    //窗口大小调整
	void HandleEvent(const sf::Event::FocusLost&);    //焦点丢失
	void HandleEvent(const sf::Event::FocusGained&);    //焦点重新获取
	void HandleEvent(const sf::Event::KeyPressed& key);    //space按下
	void HandleEvent(const sf::Event::KeyReleased& key);    //space释放
	void HandleEvent(const sf::Event::MouseButtonPressed& mouse);    //鼠标左键pressed
	void HandleEvent(const sf::Event::MouseButtonReleased& mouse);    //鼠标左键released
	void HandleEvent(const auto&) {};    //空事件
	//更新接口
	void UpdatePlayer(float deltaTime_);    //移动模块
	void UpdatePickup(float deltaTime_);    //拾取模块
	void UpdateTrap(float deltaTime_);    //陷阱模块
	void UpdateGravity(float deltaTime_);    //重力系统
	void UpdateReset();    //重置函数
	//渲染接口
	void RenderTrap();    //陷阱渲染辅助函数
	void RenderGround();    //地形绘制

	//音频接口
	void Background_Music_1();
	void Background_Music_2();
	void Background_Music_3();
	void Background_Music_4();
	void SoundCollection();
	void MusicCollection_1();
	void MusicCollection_2();
	void SoundDie();
	void SoundDie_1();
	void MusicDie_3();
	void SoundJump();
	void Background_Music_Hurdle2_1();
	void Background_Music_Hurdle2_2();
	void Back_Cycle();    //背景音乐
	void Easter_egg1();    //忘情牛肉面
	void Easter_egg2();    //急急如律令

public:
	Application();    //创建窗口构造函数

	bool IsRunning() const;    //检查窗口是否运行

	void ProcessEvents();    //事件处理
	void Update();    //更新方法
	void Render();    //渲染方法
};