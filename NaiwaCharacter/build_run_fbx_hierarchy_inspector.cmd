@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 11
cl /nologo /EHsc /MD /std:c++17 /D FBXSDK_SHARED /I "C:\Program Files\Epic Games\UE_5.8\Engine\Source\ThirdParty\FBX\2020.2\include" /I "C:\Program Files\Epic Games\UE_5.8\Engine\Source\ThirdParty\FBX\2020.2\include\fbxsdk" "C:\Users\31314\Desktop\Nai-Long-Adventure\NaiwaCharacter\fbx_hierarchy_inspector.cpp" /Fo:"C:\Users\31314\Desktop\Nai-Long-Adventure\NaiwaCharacter\fbx_hierarchy_inspector.obj" /Fe:"C:\Users\31314\Desktop\Nai-Long-Adventure\NaiwaCharacter\fbx_hierarchy_inspector.exe" /link /LIBPATH:"C:\Program Files\Epic Games\UE_5.8\Engine\Source\ThirdParty\FBX\2020.2\lib\vs2017\x64\release" libfbxsdk.lib
if errorlevel 1 exit /b 12
set "PATH=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\FBX\2020.2\Win64;%PATH%"
"C:\Users\31314\Desktop\Nai-Long-Adventure\NaiwaCharacter\fbx_hierarchy_inspector.exe" "C:\Users\31314\Desktop\Nai-Long-Adventure\NaiwaCharacter\deliveries\NaiwaEgg_S4-S5_v3_candidate\SK_NaiwaEgg.fbx" "C:\Users\31314\Desktop\Nai-Long-Adventure\NaiwaCharacter\deliveries\NaiwaEgg_S4-S5_v3_candidate\SK_NaiwaEgg_rootfixed.fbx"
