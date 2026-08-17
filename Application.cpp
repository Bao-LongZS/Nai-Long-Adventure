#include"Application.h"
#include<iostream>

//构造函数//
Application::Application() :
	window_(sf::VideoMode({ 1920,1080 }), "Nai Long Adventure")
{
	//设置窗口大小范围
	window_.setMinimumSize(sf::Vector2u(200, 300));
	window_.setMaximumSize(sf::Vector2u(2560, 1600));
	//设置垂直同步帧率
	window_.setVerticalSyncEnabled(true);

	//禁用连续键
	window_.setKeyRepeatEnabled(false);

	//纹理
	T_player_.loadFromFile("player.png");    //主角纹理
	T_collection_1.loadFromFile("collection1.png");    //拾取物状态1纹理
	T_collection_2.loadFromFile("collection2.png");    //拾取物状态2纹理
	T_BackGround_1.loadFromFile("Snow_Filed.png");    //背景1纹理
	T_BackGround_2.loadFromFile("Desert.png");    //背景2纹理
	T_BackGround_3.loadFromFile("Forest.png");    //背景3纹理

	//形状
	player_.setTexture(&T_player_);
	player_.setSize({ 70,84 });
	player_.setPosition({ 700,400 });
	player_.setOrigin(player_.getGeometricCenter());
	player_.setScale({ 2.5f,2.5f });
	
	collection_1.setTexture(&T_collection_1);
	collection_1.setSize({ 100,100 });
	collection_1.setPosition({ 1000,500 });
    collection_1.setOrigin(collection_1.getGeometricCenter());

	BackGround_.setTexture(&T_BackGround_3);
	BackGround_.setSize({ 1920, 1080 });
	BackGround_.setPosition({ 960 , 540 });
	BackGround_.setOrigin(BackGround_.getGeometricCenter());
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
	deltaTime_ = clock_.restart().asSeconds();    //每帧时间差

	UpdatePlayer(deltaTime_);    //移动方法

	UpdatePickup(deltaTime_);    //拾取方法
}


//渲染方法//
void Application::Render() 
{
	window_.clear();
	window_.draw(BackGround_);
	window_.draw(player_);
	window_.draw(collection_1);
	window_.display();
}


//私有接口//
//事件接口
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


//更新接口
void Application::UpdatePlayer(float deltaTime_) 
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) 
	{
		player_.move({0 , -400 * deltaTime_});
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
	{
		player_.move({-400 * deltaTime_ , 0});
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
	{
		player_.move({0 , 400 * deltaTime_});
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
	{
		player_.move({400 * deltaTime_ , 0});
	}
}

void Application::UpdatePickup(float deltaTime)
{
	if (pickupState_ == 0)
	{
		if (player_.getGlobalBounds().findIntersection(collection_1.getGlobalBounds())) 
		{
			collection_1.setTexture(&T_collection_2);
			collection_1.setScale({ 1.3f , 1.3f });
			pickupState_ = 1;    //开始谈出
			fadeTime_ = 0.0f;    //启动计时
		}
	}
	else if (pickupState_ == 1) 
	{
		fadeTime_ += deltaTime_;    //记录时间
		float progress = fadeTime_ / 1.5f;    //设置谈出速度
		
		if (progress < 1.0f) 
		{
			std::uint8_t alpha = static_cast<std::uint8_t>(255 - (progress * 255));
			collection_1.setFillColor(sf::Color(255, 255, 255, alpha));
		}
	}
	else 
	{
		pickupState_ = 2;
	}
}