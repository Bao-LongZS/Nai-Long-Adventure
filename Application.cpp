#include"Application.h"
#include<iostream>

//构造函数//
Application::Application() :
	window_(sf::VideoMode({ 1920,1080 }), "Nai Long Adventure"),
	B_Collection_("Collection.wav"),
	B_Die_("Die.wav"),
	B_Die_1("Die_1.wav"),
	B_Jump_("Jump.wav"),
	M_Background_1("Background_Music_1.wav"),
	M_Background_2("Background_Music_2.wav"),
	M_Background_3("Background_Music_3.wav"),
	M_Background_4("Background_Music_4.wav"),
	M_Collection_1("Collection_1.wav"),
	M_Collection_2("Collection_2.wav"),
	M_Die_3("Die_3.wav"),
	M_Hurdle2_1("Hurdle2_1.wav"),
	M_Hurdle2_2("Hurdle2_2.wav"),
	S_Collection_(B_Collection_),
	S_Die_(B_Die_),
	S_Die_1(B_Die_1),
	S_Jump_(B_Jump_)
{
	//设置窗口大小范围
	window_.setMinimumSize(sf::Vector2u(200, 300));
	window_.setMaximumSize(sf::Vector2u(2560, 1600));
	//设置垂直同步帧率
	window_.setVerticalSyncEnabled(true);

	//禁用连续键
	window_.setKeyRepeatEnabled(false);
	
	Background_Music_1();    //启动背景音乐

	//纹理//
	T_player_.loadFromFile("player.png");    //主角纹理
	T_player_glance_.loadFromFile("Glance.png");    //主角惊鸿一瞥纹理
	T_player_big_laugh_.loadFromFile("Big_Laugh.png");    //主角捧腹大笑纹理
	T_collection_1.loadFromFile("collection1.png");    //拾取物状态1纹理
	T_collection_2.loadFromFile("collection2.png");    //拾取物状态2纹理
	T_Spine_.loadFromFile("Spine.png");    //陷阱尖刺纹理
	T_Deco_tree_.loadFromFile("Tree.png");    //装饰物:树纹理
	T_Deco_Leave_Left_.loadFromFile("Leave_Left.png");    //装饰物:左叶片纹理
	T_Deco_Leave_Right_.loadFromFile("Leave_Right.png");    //装饰物:右叶片纹理
	T_Deco_cloud_mass_.loadFromFile("Cloud_Mass.png");    //装饰物:云朵纹理
	T_BackGround_1.loadFromFile("Snow.png");    //背景1纹理
	T_BackGround_2.loadFromFile("Desert.png");    //背景2纹理
	T_BackGround_3.loadFromFile("Forest.png");    //背景3纹理
	T_S_Ground_1.loadFromFile("Snow_Ground1.png");    //冬天地面1纹理
	T_S_Ground_2.loadFromFile("Snow_Ground2.png");    //冬天地面2纹理
	T_S_Ground_3.loadFromFile("Snow_Ground3.png");    //冬天地面3纹理

	//形状//
	//角色
	player_.setTexture(&T_player_);
	player_.setSize({ 70.0f,84.0f });
	player_.setPosition({ 70.0f,0.0f });
	player_.setOrigin(player_.getGeometricCenter());
	player_.setScale({ 2.0f,2.0f });
	
	//拾取物
	collection_.setTexture(&T_collection_1);
	collection_.setSize({ 80.0f,80.0f });
	collection_.setPosition({ 1395.0f,100.0f });
    collection_.setOrigin(collection_.getGeometricCenter());

	//陷阱
	sf::RectangleShape spine1;
	spine1.setTexture(&T_Spine_);
	spine1.setSize({ 130.0f,30.0f });
	spine1.setPosition({ 196.0f,810.0f });
	spine1.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine1);

	sf::RectangleShape spine2;
	spine2.setTexture(&T_Spine_);
	spine2.setSize({ 130.0f,30.0f });
	spine2.setPosition({ 1400.0f,700.0f });
	spine2.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine2);

	sf::RectangleShape spine3;
	spine3.setTexture(&T_Spine_);
	spine3.setSize({ 90.0f,20.0f });
	spine3.setPosition({ 248.0f,210.0f });
	spine3.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine3);

	sf::RectangleShape spine4;
	spine4.setTexture(&T_Spine_);
	spine4.setSize({ 90.0f,20.0f });
	spine4.setPosition({ 1535.0f,310.0f });
	spine4.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine4);

	sf::RectangleShape spine5;
	spine5.setTexture(&T_Spine_);
	spine5.setSize({ 110.0f,30.0f });
	spine5.setPosition({ 730.0f,446.0f });
	spine5.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine5);

	sf::RectangleShape spine6;
	spine6.setTexture(&T_Spine_);
	spine6.setSize({ 110.0f,30.0f });
	spine6.setPosition({ 1018.0f,446.0f });
	spine6.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine6);

	sf::RectangleShape spine7;
	spine7.setTexture(&T_Spine_);
	spine7.setSize({ 130.0f,30.0f });
	spine7.setPosition({ 1068.0f,930.0f });
	spine7.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine7);

	sf::RectangleShape spine8;
	spine8.setTexture(&T_Spine_);
	spine8.setSize({ 130.0f,30.0f });
	spine8.setPosition({ 1590.0f,810.0f });
	spine8.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine8);

	sf::RectangleShape spine9;
	spine9.setTexture(&T_Spine_);
	spine9.setSize({ 130.0f,30.0f });
	spine9.setPosition({ 1240.0f,810.0f });
	spine9.setFillColor({ 255,255,255,0 });
	Trap_.push_back(spine9);

	//背景//
	BackGround_.setTexture(&T_BackGround_1);
	BackGround_.setSize({ 1920, 1080 });
	BackGround_.setPosition({ 0 , 0 });

	//地面
	sf::RectangleShape ground1;
	ground1.setTexture(&T_S_Ground_1);
	ground1.setSize({ 174.f,120.f });
	ground1.setPosition({ 0.0f,960.f });
	Grounds_.push_back(ground1);

	sf::RectangleShape ground2;
	ground2.setTexture(&T_S_Ground_2);
	ground2.setSize({ 174.f,240.f });
	ground2.setPosition({ 174.0f,840.f });
	Grounds_.push_back(ground2);

	sf::RectangleShape ground3;
	ground3.setTexture(&T_S_Ground_3);
	ground3.setSize({ 174.f,360.f });
	ground3.setPosition({ 348.0f,730.f });
	Grounds_.push_back(ground3);

	sf::RectangleShape ground4;
	ground4.setTexture(&T_S_Ground_2);
	ground4.setSize({ 174.f,240.f });
	ground4.setPosition({ 522.0f,840.f });
	Grounds_.push_back(ground4);

	sf::RectangleShape ground5;
	ground5.setTexture(&T_S_Ground_1);
	ground5.setSize({ 174.f,120.f });
	ground5.setPosition({ 696.0f,960.f });
	Grounds_.push_back(ground5);

	sf::RectangleShape ground6;
	ground6.setTexture(&T_S_Ground_1);
	ground6.setSize({ 174.f,120.f });
	ground6.setPosition({ 870.0f,960.f });
	Grounds_.push_back(ground6);

	sf::RectangleShape ground7;
	ground7.setTexture(&T_S_Ground_1);
	ground7.setSize({ 174.f,120.f });
	ground7.setPosition({ 1044.0f,960.f });
	Grounds_.push_back(ground7);

	sf::RectangleShape ground8;
	ground8.setTexture(&T_S_Ground_2);
	ground8.setSize({ 174.f,240.f });
	ground8.setPosition({ 1218.0f,840.f });
	Grounds_.push_back(ground8);

	sf::RectangleShape ground9;
	ground9.setTexture(&T_S_Ground_3);
	ground9.setSize({ 174.f,360.f });
	ground9.setPosition({ 1392.0f,730.f });
	Grounds_.push_back(ground9);

	sf::RectangleShape ground10;
	ground10.setTexture(&T_S_Ground_2);
	ground10.setSize({ 174.f,240.f });
	ground10.setPosition({ 1566.0f,840.f });
	Grounds_.push_back(ground10);

	sf::RectangleShape ground11;
	ground11.setTexture(&T_S_Ground_1);
	ground11.setSize({ 180.f,120.f });
	ground11.setPosition({ 1740.0f,960.f });
	Grounds_.push_back(ground11);

	//装饰物
	sf::RectangleShape tree;
	tree.setTexture(&T_Deco_tree_);
	tree.setSize({ 174.0f,667.0f });
	tree.setPosition({ 850.0f,293.0f });
	Grounds_.push_back(tree);

	sf::RectangleShape leave_1;
	leave_1.setTexture(&T_Deco_Leave_Left_);
	leave_1.setSize({ 130.0f,130.0f });
	leave_1.setPosition({ 720.0f,476.0f });
	Grounds_.push_back(leave_1);

	sf::RectangleShape leave_2;
	leave_2.setTexture(&T_Deco_Leave_Right_);
	leave_2.setSize({ 130.0f,130.0f });
	leave_2.setPosition({ 1008.0f,476.0f });
	Grounds_.push_back(leave_2);

	sf::RectangleShape cloud_mass;
	cloud_mass.setTexture(&T_Deco_cloud_mass_);
	cloud_mass.setSize({ 100.0f,27.0f });
	cloud_mass.setPosition({ 1330.0f,130.0f });
	Grounds_.push_back(cloud_mass);

	sf::RectangleShape cloud_mass_1;
	cloud_mass_1.setTexture(&T_Deco_cloud_mass_);
	cloud_mass_1.setSize({ 100.0f,27.0f });
	cloud_mass_1.setPosition({ 245.0f,230.0f });
	Grounds_.push_back(cloud_mass_1);

	sf::RectangleShape cloud_mass_2;
	cloud_mass_2.setTexture(&T_Deco_cloud_mass_);
	cloud_mass_2.setSize({ 100.0f,27.0f });
	cloud_mass_2.setPosition({ 1530.0f,330.0f });
	Grounds_.push_back(cloud_mass_2);
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
	UpdateTrap(deltaTime_);    //陷阱方法
	UpdateGravity(deltaTime_);    //重力系统
	UpdateReset();    //重启方法
	Back_Cycle();    //背景音乐
}


//渲染方法//
void Application::Render() 
{
	window_.clear();
	window_.draw(BackGround_);
	RenderTrap();
	RenderGround();
	window_.draw(player_);
	window_.draw(collection_);
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
	if (key.code == sf::Keyboard::Key::Space) 
	{
		std::cout << "space pressed\n";
		JumpRequested_ = true;
	}
}

void Application::HandleEvent(const sf::Event::KeyReleased& key)
{
	//记录space释放
	if (key.code == sf::Keyboard::Key::Space) 
	{
		std::cout << "space released\n";
		JumpRequested_ = false;
	}
	
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
//移动模块
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

//拾取模块
void Application::UpdatePickup(float deltaTime_)
{
	if (pickupState_ == 0)
	{
		if (player_.getGlobalBounds().findIntersection(collection_.getGlobalBounds())) 
		{
			collection_.setTexture(&T_collection_2);
			collection_.setScale({ 1.3f , 1.3f });
			SoundCollection();    //哇哦音效
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
			collection_.setFillColor(sf::Color(255, 255, 255, alpha));
		}
	}
	else 
	{
		pickupState_ = 2;
	}
}

//陷阱模块
void Application::UpdateTrap(float deltaTime_) 
{
	sf::FloatRect playerBounds = player_.getGlobalBounds();
	sf::FloatRect trapBounds;

	//碰撞检测
	for (const auto& trap : Trap_)
	{
		if (playerBounds.findIntersection(trap.getGlobalBounds()))
		{
			isTrapTouch = true;
			trapBounds = trap.getGlobalBounds();
			break;
		}
	}

	if (playerState_ == 0)
	{
		if (playerBounds.findIntersection(trapBounds))
		{
			player_.setTexture(&T_player_glance_);
			//player_.setScale({ 1.3f , 1.3f });
			SoundDie();    //呃啊音效
			playerState_ = 1;    //开始谈出
			playerFadeTime_ = 0.0f;    //启动计时
		}
	}
	else if (playerState_ == 1)
	{
		playerFadeTime_ += deltaTime_;    //记录时间
		float progress = playerFadeTime_ / 2.5f;    //设置谈出速度
		
		if (progress >= 0.5f) 
		{
			player_.setTexture(&T_player_big_laugh_);
			SoundDie_1();    //奶龙笑音效
		}

		if (progress < 1.0f)
		{
			std::uint8_t alpha = static_cast<std::uint8_t>(255 - (progress * 255));
			player_.setFillColor(sf::Color(255, 255, 255, alpha));
		}
		if (progress >= 1.0f) 
		{
			playerState_ = 2;
		}
	}
	else
	{
		isRestart = true;
	}
}

//重力系统
void Application::UpdateGravity(float deltaTime_)
{
	const float Gravity = 2200.0f;    //重力系数
	const float Max_Fall_Speed = 1600.0f;    //最大下落速度
	const float Jump_Speed = -1500.0f;    //跳跃初速度

	velocity_Y += Gravity * deltaTime_;    //计算垂直速度

	//限速判断
	if (velocity_Y >= Max_Fall_Speed)
	{
		velocity_Y = Max_Fall_Speed;
	}

	bool isColliding = false;
	sf::FloatRect playerNextBounds = player_.getGlobalBounds();
	sf::FloatRect GroundBounds;

	//角色预测碰撞盒位置更新
	playerNextBounds.position.y += velocity_Y * deltaTime_;

	//碰撞检测
	for (const auto& ground : Grounds_)
	{
		if (playerNextBounds.findIntersection(ground.getGlobalBounds()))
		{
			GroundBounds = ground.getGlobalBounds();
			isColliding = true;
			break;
		}
	}

	if (isColliding)
	{
		//吸附地面
		if (velocity_Y > 0.0f)
		{
			player_.setPosition({ player_.getPosition().x ,GroundBounds.position.y - player_.getSize().y });
		}

		//落地参数重置
		velocity_Y = 0.0f;
		isOnGround_ = true;

		//跳跃事件
		if (JumpRequested_)
		{
			if (isOnGround_)
			{
				SoundJump();    //hey跳跃音效
				velocity_Y = Jump_Speed;
				isOnGround_ = false;
				player_.move({ 0.0f , -18.0f });
			}
			JumpRequested_ = false;
		}

	}
	else
	{
		player_.move({ 0, velocity_Y * deltaTime_ });
	}
}

//重置函数
void Application::UpdateReset() 
{
	if (isRestart) 
	{
		//重置主角状态位置
		player_.setTexture(&T_player_);
		player_.setFillColor({ 255,255,255,255 });
		player_.setPosition({ 70.0f,200.0f });
	    playerState_ = 0;

	    //重置陷阱状态
	    for (auto& spine : Trap_)
	    {
		    spine.setFillColor({ 255,255,255,0 });
	    }
	    isTrapTouch = false;

	    //重置拾取物状态
		collection_.setTexture(&T_collection_1);
		collection_.setScale({ 1.0f , 1.0f });
		collection_.setFillColor({ 255,255,255,255 });
	    pickupState_ = 0;
		//彩蛋标记
		Easter_egg_++;
	}
	isRestart = false;
}

//渲染接口
//陷阱绘制
void Application::RenderTrap() 
{
	for (auto& spine : Trap_)
	{
		window_.draw(spine);
		if (isTrapTouch)
		{
			spine.setFillColor({ 255,255,255,255 });
		}
		else
		{
			spine.setFillColor({ 255,255,255,0 });
		}
	}
}

//地形绘制
void Application::RenderGround() 
{
	for (const auto& ground : Grounds_)
	{
		window_.draw(ground);
	}
}

//音频接口
void Application::Background_Music_1()
{
	M_Background_1.play();
	M_Background_1.setVolume(30);
	M_Background_1.setPitch(1.2);
}

void Application::Background_Music_2()
{
	M_Background_2.play();
	M_Background_2.setVolume(30);
}

void Application::Background_Music_3()
{
	M_Background_3.play();
	M_Background_3.setVolume(30);
}

void Application::Background_Music_4()
{
	M_Background_4.play();
	M_Background_4.setVolume(20);
}

void Application::SoundCollection()
{
	S_Collection_.play();
}

void Application::MusicCollection_1()
{
	M_Collection_1.play();
	M_Collection_1.setVolume(20);
	M_Collection_1.setPitch(1.5);
}

void Application::MusicCollection_2()
{
	M_Collection_2.play();
}

void Application::SoundDie()
{
	S_Die_.play();
	S_Die_.setPitch(1.5);
}

void Application::SoundDie_1()
{
	S_Die_1.play();
}

void Application::MusicDie_3()
{
	M_Die_3.play();
	M_Die_3.setVolume(20);
	M_Die_3.setPitch(1.5);
}

void Application::SoundJump()
{
	S_Jump_.play();
}

void Application::Background_Music_Hurdle2_1()
{
	M_Hurdle2_1.play();
}

void Application::Background_Music_Hurdle2_2()
{
	M_Hurdle2_2.play();
}

//背景音乐//
void Application::Back_Cycle()
{
	// 只有当第一首【彻底停止播放】了，才去启动第二首
	if (current_music_ == 1 && M_Background_1.getStatus() == sf::Sound::Status::Stopped)
	{
		M_Background_1.stop(); // 保险起见彻底停掉第一首
		Background_Music_2(); 
		current_music_ = 2;
	}
	else if (current_music_ == 2 && M_Background_2.getStatus() == sf::Sound::Status::Stopped)
	{
		M_Background_2.stop();
		Background_Music_3();
		current_music_ = 3;
	}
	else if (current_music_ == 3 && M_Background_3.getStatus() == sf::Sound::Status::Stopped)
	{
		M_Background_3.stop();
		Background_Music_4();
		current_music_ = 4;
	}
	else if (current_music_ == 4 && M_Background_4.getStatus() == sf::Sound::Status::Stopped)
	{
		M_Background_4.stop();
		MusicCollection_1(); // 回到第一首，实现无限整体循环
		current_music_ = 5;
	}
	else if (current_music_ == 5 && M_Collection_1.getStatus() == sf::Sound::Status::Stopped)
	{
		M_Collection_1.stop();
		MusicDie_3(); // 回到第一首，实现无限整体循环
		current_music_ = 6;
	}
	else if (current_music_ == 6 && M_Die_3.getStatus() == sf::Sound::Status::Stopped)
	{
		M_Die_3.stop();
		Background_Music_1(); // 回到第一首，实现无限整体循环
		current_music_ = 1;
	}
}

