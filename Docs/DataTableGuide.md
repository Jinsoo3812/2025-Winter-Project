# 데이터 테이블 시스템 가이드

## 개요
이 프로젝트는 하드코딩을 제거하고 데이터 테이블을 통해 게임 밸런스를 쉽게 조정할 수 있도록 설계되었습니다.

---

## 1. 플레이어 초기 스탯 데이터 테이블

### 구조체: `FPlayerInitialStatsRow`
- **위치**: `Source/Winter2025/Public/Player/PlayerAttributeSet.h`
- **용도**: 플레이어의 레벨별 초기 능력치 정의

### 필드
| 필드명 | 타입 | 설명 |
|--------|------|------|
| Level | int32 | 플레이어 레벨 (Row Name으로도 사용) |
| InitialHealth | float | 초기 체력 |
| InitialMaxHealth | float | 초기 최대 체력 |
| InitialMana | float | 초기 마나 |
| InitialMaxMana | float | 초기 최대 마나 |
| InitialAttackPower | float | 초기 공격력 |
| InitialMovementSpeed | float | 초기 이동속도 |

### 사용 방법
1. **에디터에서 데이터 테이블 생성**
   - Content Browser → 우클릭 → Miscellaneous → Data Table
   - Row Structure: `PlayerInitialStatsRow` 선택
   - 이름: `DT_PlayerStats` (예시)

2. **데이터 입력**
   - Row Name: `1`, `2`, `3`... (레벨 숫자)
   - 각 필드에 값 입력

3. **블루프린트에서 설정**
   - `BP_TestPlayerState` 블루프린트 열기
   - Details 패널 → Data|Player Stats → Player Stats Data Table
   - 생성한 데이터 테이블 할당
   - Player Level 설정 (기본값: 1)

### CSV 예시
```csv
---,Level,InitialHealth,InitialMaxHealth,InitialMana,InitialMaxMana,InitialAttackPower,InitialMovementSpeed
1,1,100,100,100,100,10,600
2,2,120,120,120,120,12,600
3,3,150,150,150,150,15,600
```

---

## 2. 룬 데이터 테이블

### 구조체: `FRuneDataRow`
- **위치**: `Source/Skill/Public/Rune/DA_Rune.h`
- **용도**: 룬의 정보 및 효과 정의

### 필드
| 필드명 | 타입 | 설명 |
|--------|------|------|
| RuneName | FText | 룬 이름 (UI용) |
| RuneAsset | UDA_Rune* | 실제 룬 데이터 에셋 |
| Description | FText | 룬 설명 |
| RuneIcon | UTexture2D* | 룬 아이콘 (UI용) |

### DA_Rune (DataAsset) 구조
| 필드명 | 타입 | 설명 |
|--------|------|------|
| RuneTag | FGameplayTag | 룬 타입 태그 (Rune.Red, Rune.Yellow, Rune.Blue, Rune.Green) |
| RuneType | ERuneType | 룬 타입 Enum |
| RuneValue | float | 강화값 (예: 1.5 = 150% 데미지) |
| OriginalSkillClass | TSubclassOf<UGameplayAbility> | [초록룬 전용] 변환 대상 스킬 |
| ReplacementSkillClass | TSubclassOf<UGameplayAbility> | [초록룬 전용] 변환 후 스킬 |

### 룬 타입별 사용법

#### 빨강 룬 (데미지 증폭)
- RuneTag: `Rune.Red`
- RuneType: `Red`
- RuneValue: `1.5` (50% 데미지 증가)
- OriginalSkillClass: 비워둠
- ReplacementSkillClass: 비워둠

#### 노랑 룬 (쿨타임 감소)
- RuneTag: `Rune.Yellow`
- RuneType: `Yellow`
- RuneValue: `0.2` (20% 쿨타임 감소)
- OriginalSkillClass: 비워둠
- ReplacementSkillClass: 비워둠

#### 파랑 룬 (범위 증폭)
- RuneTag: `Rune.Blue`
- RuneType: `Blue`
- RuneValue: `1.3` (30% 범위 증가)
- OriginalSkillClass: 비워둠
- ReplacementSkillClass: 비워둠

#### 초록 룬 (스킬 변환)
- RuneTag: `Rune.Green`
- RuneType: `Green`
- RuneValue: `1.0` (사용 안 함)
- OriginalSkillClass: `BP_GA_Destruction` (변환 대상 스킬)
- ReplacementSkillClass: `BP_GA_SpinDestruction` (변환 후 스킬)

### 사용 방법
1. **룬 데이터 에셋 생성**
   - Content Browser → 우클릭 → Blueprint Class → Data Asset
   - Parent Class: `DA_Rune`
   - 이름: `DA_Rune_Red_Damage_150` (예시)
   - 필드 설정 (위 표 참조)

2. **데이터 테이블 생성**
   - Content Browser → 우클릭 → Miscellaneous → Data Table
   - Row Structure: `RuneDataRow`
   - 이름: `DT_Runes`

3. **데이터 입력**
   - Row Name: `Rune_Red_001`, `Rune_Green_Transform_001` 등
   - RuneAsset 필드에 생성한 DA_Rune 할당

4. **SkillManagerComponent에서 사용**
   ```cpp
   // C++에서 룬 장착 (ID 기반)
   SkillManager->EquipRuneByID(SlotIndex, RuneSlotIndex, FName("Rune_Red_001"));
   ```

### CSV 예시
```csv
---,RuneName,RuneAsset,Description,RuneIcon
Rune_Red_001,"빨강 룬 (150%)",DA_Rune_Red_Damage_150,"데미지 50% 증가",T_Rune_Red
Rune_Green_001,"초록 룬 (파괴→회전파괴)",DA_Rune_Green_Spin,"파괴 스킬을 회전파괴로 변환",T_Rune_Green
```

---

## 3. 스킬 데이터 테이블 (선택사항)

### 구조체: `FSkillDataRow`
- **위치**: `Source/Skill/Public/Data/SkillDataStructs.h`
- **용도**: 스킬의 기본 정보 및 스탯 정의 (선택적)

### 필드
| 필드명 | 타입 | 설명 |
|--------|------|------|
| SkillName | FText | 스킬 이름 (UI용) |
| SkillClass | TSubclassOf<UGameplayAbility> | 스킬 GA 클래스 |
| BaseDamage | float | 기본 피해량 |
| BaseCooldown | float | 기본 쿨타임 (초) |
| BaseRange | float | 기본 범위 계수 |
| Description | FText | 스킬 설명 |
| SkillIcon | UTexture2D* | 스킬 아이콘 |

### 사용 방법
스킬 데이터 테이블은 **선택적**입니다. 대부분의 경우 스킬은 블루프린트 클래스로 정의되며, 각 블루프린트에서 BaseDamage, BaseCooldown, BaseRange를 직접 설정합니다.

데이터 테이블을 사용하려면:
1. 데이터 테이블 생성 (Row Structure: `SkillDataRow`)
2. 스킬 정보 입력
3. 필요시 C++에서 로드하여 사용

---

## 4. 초록룬 스킬 변환 시스템

### 작동 원리
1. `TestPlayerState::InitializeSkills()` 호출
2. 각 스킬 슬롯의 초록룬 감지
3. 초록룬의 `OriginalSkillClass`와 현재 장착된 스킬 비교
4. 일치하면 `ReplacementSkillClass`로 스킬 교체
5. SkillManager에 변환된 스킬 장착

### 예시: 파괴 스킬 → 회전파괴 스킬
1. **초록룬 DA 생성**: `DA_Rune_Green_Spin`
   - RuneType: `Green`
   - OriginalSkillClass: `BP_GA_Destruction`
   - ReplacementSkillClass: `BP_GA_SpinDestruction`

2. **스킬 슬롯 설정**:
   - Slot 0: EquippedSkill = `BP_GA_Destruction`
   - Rune Slot 0: RuneAsset = `DA_Rune_Green_Spin`

3. **결과**: 게임 시작 시 자동으로 `BP_GA_SpinDestruction`이 장착됨

### 장점
- ? 하드코딩 제거 (기존: `if (RuneValue == 1.0f)`)
- ? 데이터 기반 스킬 변환
- ? 여러 개의 초록룬을 쉽게 추가 가능
- ? 블루프린트에서 설정만으로 새로운 변환 룬 생성

---

## 5. 마이그레이션 가이드

### 기존 프로젝트에서 변경된 사항

#### TestPlayerState
- ? 제거: `SpinDestructionSkillClass` 프로퍼티
- ? 제거: 하드코딩된 `if (RuneValue == 1.0f)` 로직
- ? 추가: `PlayerStatsDataTable` 프로퍼티
- ? 추가: `PlayerLevel` 프로퍼티
- ? 추가: `InitializePlayerStats()` 함수

#### PlayerAttributeSet
- ? 추가: `Mana`, `MaxMana`, `AttackPower`, `MovementSpeed` 속성
- ? 추가: `FPlayerInitialStatsRow` 구조체

#### DA_Rune
- ? 추가: `RuneType` Enum
- ? 추가: `OriginalSkillClass` 프로퍼티
- ? 추가: `ReplacementSkillClass` 프로퍼티

### 블루프린트 업데이트 필요사항

1. **BP_TestPlayerState**:
   - Player Stats Data Table 할당
   - Player Level 설정

2. **기존 초록룬 DA**:
   - RuneType을 `Green`으로 설정
   - OriginalSkillClass 설정
   - ReplacementSkillClass 설정

3. **기존 빨강/노랑/파랑 룬 DA**:
   - RuneType 설정 (Red/Yellow/Blue)

---

## 6. 테스트 방법

### 플레이어 스탯 테스트
1. DT_PlayerStats 생성 및 데이터 입력
2. BP_TestPlayerState에서 데이터 테이블 할당
3. Player Level 변경 (1, 2, 3...)
4. 게임 실행 후 로그 확인:
   ```
   LogTemp: ATestPlayerState: Loaded stats for Level 1 - HP: 100, Mana: 100, ...
   ```

### 초록룬 스킬 변환 테스트
1. DA_Rune_Green 생성:
   - OriginalSkillClass: BP_GA_Destruction
   - ReplacementSkillClass: BP_GA_SpinDestruction

2. BP_TestPlayerState → DefaultSkillSets:
   - Slot 0 → EquippedSkill: BP_GA_Destruction
   - Slot 0 → RuneSlots[0] → RuneAsset: DA_Rune_Green

3. 게임 실행 후 로그 확인:
   ```
   LogTemp: [Slot 0] Green Rune detected: Replacing BP_GA_Destruction with BP_GA_SpinDestruction
   ```

4. 게임 플레이 중 해당 스킬 사용 시 회전파괴 스킬이 발동되는지 확인

---

## 7. 자주 묻는 질문 (FAQ)

### Q: 데이터 테이블 없이도 작동하나요?
A: 네, PlayerStatsDataTable이 null이면 기본값을 사용합니다 (Health: 100, Mana: 100 등).

### Q: 런타임에 플레이어 레벨을 변경하려면?
A: `TestPlayerState::InitializePlayerStats(NewLevel)` 함수를 호출하세요.

### Q: 초록룬 여러 개를 동시에 장착하면?
A: SkillManagerComponent가 자동으로 마지막 초록룬만 유지합니다.

### Q: 스킬 데이터 테이블은 필수인가요?
A: 아니요. 블루프린트에서 직접 BaseDamage 등을 설정하는 것이 더 일반적입니다.

### Q: CSV 파일로 데이터 테이블을 만들려면?
A: 위의 CSV 예시를 복사하여 `.csv` 파일로 저장 → 에디터에서 Import 하세요.

---

## 8. 향후 확장 가능성

- 플레이어 직업별 초기 스탯 (Row Name: `Warrior_1`, `Mage_1` 등)
- 레벨업 시 자동 스탯 증가
- 스킬 업그레이드 시스템 (레벨별 데미지/쿨타임 데이터 테이블)
- 장비 시스템 (장비별 스탯 보너스 데이터 테이블)

---

**작성일**: 2025-01-XX  
**버전**: 1.0  
**작성자**: GitHub Copilot
