#include <fbxsdk.h>
#include <cstring>
#include <iomanip>
#include <iostream>

static const char* AttributeName(const FbxNode* Node)
{
	const FbxNodeAttribute* Attribute = Node ? Node->GetNodeAttribute() : nullptr;
	if (!Attribute) return "none";
	switch (Attribute->GetAttributeType())
	{
	case FbxNodeAttribute::eSkeleton: return "skeleton";
	case FbxNodeAttribute::eMesh: return "mesh";
	case FbxNodeAttribute::eNull: return "null";
	default: return "other";
	}
}

static void PrintNode(FbxNode* Node, const int Depth)
{
	if (!Node) return;
	const FbxAMatrix Global = Node->EvaluateGlobalTransform();
	const FbxVector4 Translation = Global.GetT();
	const FbxVector4 Rotation = Global.GetR();
	const FbxVector4 Scale = Global.GetS();
	const FbxVector4 LocalTranslation = Node->LclTranslation.Get();
	const FbxVector4 LocalRotation = Node->LclRotation.Get();
	const FbxVector4 LocalScale = Node->LclScaling.Get();
	const FbxVector4 PreRotation = Node->GetPreRotation(FbxNode::eSourcePivot);
	const FbxVector4 PostRotation = Node->GetPostRotation(FbxNode::eSourcePivot);
	std::cout << std::string(Depth * 2, ' ') << Node->GetName()
		<< " attr=" << AttributeName(Node)
		<< std::fixed << std::setprecision(6)
		<< " globalT=(" << Translation[0] << "," << Translation[1] << "," << Translation[2] << ")"
		<< " globalR=(" << Rotation[0] << "," << Rotation[1] << "," << Rotation[2] << ")"
		<< " globalS=(" << Scale[0] << "," << Scale[1] << "," << Scale[2] << ")"
		<< " localT=(" << LocalTranslation[0] << "," << LocalTranslation[1] << "," << LocalTranslation[2] << ")"
		<< " localR=(" << LocalRotation[0] << "," << LocalRotation[1] << "," << LocalRotation[2] << ")"
		<< " localS=(" << LocalScale[0] << "," << LocalScale[1] << "," << LocalScale[2] << ")"
		<< " preR=(" << PreRotation[0] << "," << PreRotation[1] << "," << PreRotation[2] << ")"
		<< " postR=(" << PostRotation[0] << "," << PostRotation[1] << "," << PostRotation[2] << ")"
		<< " children=" << Node->GetChildCount() << "\n";
	for (int Child = 0; Child < Node->GetChildCount(); ++Child)
	{
		PrintNode(Node->GetChild(Child), Depth + 1);
	}
}

static FbxNode* FindChildByName(FbxNode* Parent, const char* Name)
{
	if (!Parent) return nullptr;
	for (int Child = 0; Child < Parent->GetChildCount(); ++Child)
	{
		FbxNode* Candidate = Parent->GetChild(Child);
		if (std::strcmp(Candidate->GetName(), Name) == 0) return Candidate;
		if (FbxNode* Nested = FindChildByName(Candidate, Name)) return Nested;
	}
	return nullptr;
}

static bool CollapseArmatureWrapper(FbxScene* Scene)
{
	FbxNode* SceneRoot = Scene->GetRootNode();
	FbxNode* Wrapper = FindChildByName(SceneRoot, "SK_NaiwaEgg_Skeleton");
	if (!Wrapper || Wrapper->GetNodeAttribute() == nullptr ||
		Wrapper->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eNull ||
		Wrapper->GetChildCount() != 1)
	{
		std::cerr << "Expected a single-child Null armature wrapper.\n";
		return false;
	}
	FbxNode* Bone = Wrapper->GetChild(0);
	if (!Bone->GetNodeAttribute() || Bone->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton ||
		std::strcmp(Bone->GetName(), "Root") != 0)
	{
		std::cerr << "Expected the wrapper's only child to be the Root skeleton bone.\n";
		return false;
	}
	FbxNode* OldParent = Wrapper->GetParent();
	if (!OldParent)
	{
		std::cerr << "Armature wrapper has no parent node.\n";
		return false;
	}

	const FbxAMatrix OldBoneGlobal = Bone->EvaluateGlobalTransform();
	const FbxVector4 GlobalTranslation = OldBoneGlobal.GetT();
	const FbxVector4 GlobalRotation = OldBoneGlobal.GetR();
	const FbxVector4 GlobalScale = OldBoneGlobal.GetS();

	for (int PoseIndex = 0; PoseIndex < Scene->GetPoseCount(); ++PoseIndex)
	{
		FbxPose* Pose = Scene->GetPose(PoseIndex);
		for (int Entry = Pose->GetCount() - 1; Entry >= 0; --Entry)
		{
			if (Pose->GetNode(Entry) == Wrapper)
			{
				Pose->Remove(Entry);
			}
			else if (Pose->GetNode(Entry) == Bone && Pose->IsLocalMatrix(Entry))
			{
				Pose->SetMatrix(Entry, OldBoneGlobal);
			}
		}
	}

	OldParent->RemoveChild(Wrapper);
	Wrapper->RemoveChild(Bone);
	OldParent->AddChild(Bone);
	Bone->LclTranslation.Set(GlobalTranslation);
	Bone->LclRotation.Set(GlobalRotation);
	Bone->LclScaling.Set(GlobalScale);
	Bone->SetPreRotation(FbxNode::eSourcePivot, FbxVector4(0.0, 0.0, 0.0));
	Bone->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(0.0, 0.0, 0.0));
	Bone->SetRotationPivot(FbxNode::eSourcePivot, FbxVector4(0.0, 0.0, 0.0));
	Bone->SetRotationOffset(FbxNode::eSourcePivot, FbxVector4(0.0, 0.0, 0.0));
	Bone->SetScalingPivot(FbxNode::eSourcePivot, FbxVector4(0.0, 0.0, 0.0));
	Bone->SetScalingOffset(FbxNode::eSourcePivot, FbxVector4(0.0, 0.0, 0.0));
	Wrapper->Destroy(true);

	const FbxAMatrix NewBoneGlobal = Bone->EvaluateGlobalTransform();
	for (int Row = 0; Row < 4; ++Row)
	{
		for (int Column = 0; Column < 4; ++Column)
		{
			if (std::abs(OldBoneGlobal.Get(Row, Column) - NewBoneGlobal.Get(Row, Column)) > 1.0e-5)
			{
				std::cerr << "Root global transform changed after wrapper removal.\n";
				return false;
			}
		}
	}
	if (FindChildByName(SceneRoot, "SK_NaiwaEgg_Skeleton") || Bone->GetParent() != SceneRoot)
	{
		std::cerr << "Wrapper removal did not produce a direct scene-root bone.\n";
		return false;
	}
	return true;
}

int main(int argc, char** argv)
{
	if (argc != 2 && argc != 3)
	{
		std::cerr << "usage: fbx_collapse_armature_wrapper <input.fbx> [root-fixed-output.fbx]\n";
		return 2;
	}
	FbxManager* Manager = FbxManager::Create();
	if (!Manager) return 3;
	FbxIOSettings* Settings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(Settings);
	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(argv[1], -1, Manager->GetIOSettings()))
	{
		std::cerr << Importer->GetStatus().GetErrorString() << "\n";
		Manager->Destroy();
		return 4;
	}
	FbxScene* Scene = FbxScene::Create(Manager, "Scene");
	if (!Importer->Import(Scene))
	{
		std::cerr << Importer->GetStatus().GetErrorString() << "\n";
		Manager->Destroy();
		return 5;
	}
	Importer->Destroy();
	std::cout << "FBX " << Scene->GetName() << " children=" << Scene->GetRootNode()->GetChildCount() << "\n";
	PrintNode(Scene->GetRootNode(), 0);
	if (argc == 3)
	{
		if (!CollapseArmatureWrapper(Scene))
		{
			Manager->Destroy();
			return 6;
		}
		std::cout << "After safe wrapper collapse:\n";
		PrintNode(Scene->GetRootNode(), 0);
		FbxExporter* Exporter = FbxExporter::Create(Manager, "");
		if (!Exporter->Initialize(argv[2], -1, Manager->GetIOSettings()) || !Exporter->Export(Scene))
		{
			std::cerr << Exporter->GetStatus().GetErrorString() << "\n";
			Exporter->Destroy();
			Manager->Destroy();
			return 7;
		}
		Exporter->Destroy();
		std::cout << "Wrote root-only hierarchy to " << argv[2] << "\n";
	}
	Manager->Destroy();
	return 0;
}
