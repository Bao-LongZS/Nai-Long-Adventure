#include"Application.h"
#include<iostream>

//构造函数//
Application::Application() :
	window_(sf::VideoMode({ 1024,768 }), "Nai Long Adventure")
{
	//设置窗口大小范围
	window_.setMinimumSize(sf::Vector2u(200, 300));
	window_.setMaximumSize(sf::Vector2u(2560, 1600));
	//设置垂直同步帧率
	window_.setVerticalSyncEnabled(true);
}


//检查运行状态//
bool Application::IsRunning() const 
{
	return window_.isOpen();
}


//事件处理//
void Application::ProcessEvents() 
{
	while (const std::optional<sf::Event> event = window_.pollEvent()) 
	{
		if (event->is<sf::Event::Closed>()) 
		{
			window_.close();
		}

		//分配事件
		event->visit([this](const auto& type) { this->HandleEvent(type); });
	}
}


//更新方法//
void Application::Update() 
{

}


//渲染方法//
void Application::Render() 
{
	window_.clear();
	window_.display();
}


//离散事件方法//
void Application::HandleEvent(const sf::Event::Closed) 
{
	window_.close();
	std::cout << "Window closed\n";
}

void Application::HandleEvent(const sf::Event::Resized& resized)
{
	std::cout << "New size: " << resized.size.x << "x" << resized.size.y <<std::endl;

	//窗口缩放画面处理
	sf::View view;
	sf::Vector2f NewSize(static_cast<float>(resized.size.x), static_cast<float>(resized.size.y));
	view.setSize(NewSize);
	view.setCenter(NewSize / 2.0f);
	window_.setView(view);
}

void Application::HandleEvent(const sf::Event::FocusLost&) 
{
	//标记失焦
	std::cout << "Focus lost\n";
	isPaused_ = true;
}

void Application::HandleEvent(const sf::Event::FocusGained&)
{
	//重新获得焦点
	std::cout << "Focus gained\n";
	isPaused_ = false;
}

void Application::HandleEvent(const sf::Event::KeyPressed& key) 
{
	//记录space按下
	std::cout << "space pressed\n";
	JumpRequested_ = true;
}

void Application::HandleEvent(const sf::Event::KeyReleased& key)
{
	//记录space释放
	std::cout << "space released\n";
	JumpRequested_ = false;
}

void Application::HandleEvent(const sf::Event::MouseButtonPressed& mouse) 
{
	//发送鼠标点击的像素坐标
	if(mouse.button == sf::Mouse::Button::Left)
	{
		std::cout << "Mouse button pressed at: ";
		std::cout << mouse.position.x << ", " << mouse.position.y << std::endl;
	}
}

void Application::HandleEvent(const sf::Event::MouseButtonReleased& mouse)
{
	//发送鼠标释放的像素坐标
	if (mouse.button == sf::Mouse::Button::Left)
	{
		std::cout << "Mouse button released at: ";
		std::cout << mouse.position.x << ", " << mouse.position.y << std::endl;
	}
}