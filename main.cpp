#include"Application.h"

int main() 
{
    /*auto app = std::make_unique<Application>();

    while (app->IsRunning())
    {
        app->ProcessEvents();
        app->Update();
        app->Render();
    }*/

    Application app;
    while (app.IsRunning()) {
        app.ProcessEvents();
        app.Update();
        app.Render();
    }
    return 0;
}