#pragma once

#include<SFML/Graphics.hpp>

class Application 
{
private:
	sf::RenderWindow window_;    //窗口
	bool isPaused_ = false;    //暂停参量
	bool JumpRequested_ = false; //跳跃参量
public:
	Application();    //创建窗口构造函数

	bool IsRunning() const;    //检查窗口是否运行

	void ProcessEvents();    //事件处理
	void Update();    //更新方法
	void Render();    //渲染方法

private:
	void HandleEvent(const sf::Event::Closed);    //关闭窗口
	void HandleEvent(const sf::Event::Resized& resized);    //窗口大小调整
	void HandleEvent(const sf::Event::FocusLost&);    //焦点丢失
	void HandleEvent(const sf::Event::FocusGained&);    //焦点重新获取
	void HandleEvent(const sf::Event::KeyPressed& key);    //space按下
	void HandleEvent(const sf::Event::KeyReleased& key);    //space释放
	void HandleEvent(const sf::Event::MouseButtonPressed& mouse);    //鼠标左键pressed
	void HandleEvent(const sf::Event::MouseButtonReleased& mouse);    //鼠标左键released
	void HandleEvent(const auto&) {};    //空事件
};