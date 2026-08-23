#pragma once

#include<SFML/Graphics.hpp>
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

public:
	Application();    //创建窗口构造函数

	bool IsRunning() const;    //检查窗口是否运行

	void ProcessEvents();    //事件处理
	void Update();    //更新方法
	void Render();    //渲染方法
};