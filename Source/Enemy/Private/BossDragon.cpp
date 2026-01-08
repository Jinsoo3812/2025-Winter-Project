// BossDragon.cpp
#include "BossDragon.h"

ABossDragon::ABossDragon()
{
	// ASC 생성 코드 삭제! 부모(EnemyBase) 생성자에서 이미 만들었음.
	// Mesh 설정이나 드래곤 전용 컴포넌트만 초기화하면 됨.
}

void ABossDragon::BeginPlay()
{
	Super::BeginPlay(); // [필수] 여기서 부모의 GAS 초기화, 스킬 부여가 다 실행됨.

	// 드래곤만의 추가 로직이 있다면 여기에 작성
}