#!/usr/bin/env bash
set -euo pipefail

# Arena60 dev-history 문서 개선 스크립트
# 실행: bash dev-history/improve-docs.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "================================================"
echo "Arena60 dev-history 문서 개선 스크립트"
echo "================================================"
echo

# 1. 파일명 표준화
echo "[1/3] 파일명 표준화 중..."
if [ -f "dev-history-bootstrap&ci-cd&1.0.md" ]; then
    mv "dev-history-bootstrap&ci-cd&1.0.md" "dev-history-bootstrap-ci-cd-1.0.md"
    echo "  ✅ bootstrap 파일명 변경 완료"
fi

# 2. 메타데이터 추가 함수
add_metadata() {
    local file=$1
    local mvp=$2
    local temp_file="${file}.tmp"

    if ! grep -q "^---$" "$file"; then
        cat > "$temp_file" <<EOF
---
version: 1.0.0
last_updated: $(date +%Y-%m-%d)
mvp: "$mvp"
status: complete
author: Arena60 Project Team
---

EOF
        cat "$file" >> "$temp_file"
        mv "$temp_file" "$file"
        echo "  ✅ $file 메타데이터 추가 완료"
    else
        echo "  ⏭️  $file 메타데이터 이미 존재"
    fi
}

# 2. 메타데이터 추가
echo
echo "[2/3] 메타데이터 추가 중..."
add_metadata "dev-history-bootstrap-ci-cd-1.0.md" "1.0"
add_metadata "dev-history-1.1.md" "1.1"
add_metadata "dev-history-1.2.md" "1.2"
add_metadata "dev-history-1.3.md" "1.3"
add_metadata "dev-history-checkpoint-a.md" "checkpoint-a"

# 3. 검증
echo
echo "[3/3] 검증 중..."
file_count=$(ls -1 dev-history-*.md 2>/dev/null | wc -l)
echo "  📄 총 문서 수: $file_count"

if [ -f "dev-history-bootstrap-ci-cd-1.0.md" ]; then
    echo "  ✅ 파일명 표준화 완료"
else
    echo "  ⚠️  파일명 확인 필요"
fi

echo
echo "================================================"
echo "✅ 개선 완료!"
echo "================================================"
echo
echo "다음 단계:"
echo "1. git status로 변경 사항 확인"
echo "2. git diff로 메타데이터 검토"
echo "3. git add dev-history/*.md"
echo "4. git commit -m 'docs: standardize dev-history metadata and filenames'"
