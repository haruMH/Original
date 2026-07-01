/* 
  script.js
  ポートフォリオWebサイトのインタラクティブ制御
  （作品実績セクションでのタブ切り替え機能など）
*/

document.addEventListener('DOMContentLoaded', () => {
    // === 1. 作品実績（Projects）のタブ切り替え制御 ===
    const tabs = document.querySelectorAll('.project-tab');
    const contents = document.querySelectorAll('.project-content');

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            // すべてのタブの active クラスを取り除く
            tabs.forEach(t => t.classList.remove('active'));
            // クリックされたタブに active クラスを追加
            tab.classList.add('active');

            // 表示するコンテンツのIDを取得
            const targetId = tab.getAttribute('data-tab');

            // すべてのコンテンツの active クラスを取り除く
            contents.forEach(content => {
                content.classList.remove('active');
            });

            // 対象のコンテンツを表示する
            const targetContent = document.getElementById(targetId);
            if (targetContent) {
                targetContent.classList.add('active');
            }
        });
    });

    // === 2. ナビゲーションリンクのスムーズスクロール補助 ===
    // (CSSの scroll-behavior: smooth が効かないブラウザへの互換対応)
    const navLinks = document.querySelectorAll('header a');
    navLinks.forEach(link => {
        link.addEventListener('click', (e) => {
            const targetId = link.getAttribute('href');
            if (targetId.startsWith('#')) {
                const targetElement = document.querySelector(targetId);
                if (targetElement) {
                    // 標準の挙動をキャンセルしてスクロールさせる（もし必要な場合）
                    // 現状はCSSでスムーズに動くため、何もしなくても問題ありません。
                }
            }
        });
    });
});
