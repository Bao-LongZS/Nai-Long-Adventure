#pragma once

#include<SFML/Graphics.hpp>

class Application
{
//系统参量
private:
	sf::RenderWindow window_;    //窗口

	bool isPaused_ = false;    //暂停参量
	bool JumpRequested_ = false; //跳跃参量

	sf::Clock clock_;    //增量时间
	float deltaTime_ = 0.f;    //时间差

	int pickupState_ = 0;    //拾取物状态标记
	float fadeTime_ = 0.f;    //淡出时间

//角色参量
private:
	//纹理//
	sf::Texture T_player_;    //主角纹理
	sf::Texture T_collection_1;    //拾取物状态1纹理
	sf::Texture T_collection_2;    //拾取物状态2纹理
	sf::Texture T_BackGround_1;    //背景1
	sf::Texture T_BackGround_2;    //背景3
	sf::Texture T_BackGround_3;    //背景2
	//形状//
	sf::RectangleShape player_;    //主角形状
	sf::RectangleShape collection_1;    //拾取物形状
	//背景//
	sf::RectangleShape BackGround_;    //背景形状

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
	void UpdatePlayer(float deltaTime);
	void UpdatePickup(float deltaTime);

public:
	Application();    //创建窗口构造函数

	bool IsRunning() const;    //检查窗口是否运行

	void ProcessEvents();    //事件处理
	void Update();    //更新方法
	void Render();    //渲染方法
};