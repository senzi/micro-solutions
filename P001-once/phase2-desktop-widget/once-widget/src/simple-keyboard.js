/**
 * Simple Keyboard - 简化的虚拟键盘实现
 * 基于simple-keyboard项目的基本功能
 */

class SimpleKeyboard {
    constructor(options = {}) {
        this.options = {
            layout: options.layout || { 
                default: [
                    "1 2 3",
                    "4 5 6", 
                    "7 8 9",
                    "0 . {bksp}"
                ]
            },
            display: options.display || {
                "{bksp}": "⌫",
                "{enter}": "↵",
                "{shift}": "⇧",
                "{space}": "Space"
            },
            theme: options.theme || "hg-theme-default",
            onChange: options.onChange || (() => {}),
            onKeyPress: options.onKeyPress || (() => {}),
            ...options
        };
        
        this.input = "";
        this.elements = {};
        this.init();
    }

    init() {
        this.createKeyboard();
        this.bindEvents();
    }

    createKeyboard() {
        const container = document.querySelector(".simple-keyboard");
        if (!container) return;

        container.className = `simple-keyboard ${this.options.theme}`;
        container.innerHTML = "";

        const layout = this.options.layout.default;
        
        layout.forEach(row => {
            const rowDiv = document.createElement("div");
            rowDiv.className = "hg-row";
            
            const buttons = row.split(" ");
            buttons.forEach(button => {
                const buttonDiv = document.createElement("div");
                buttonDiv.className = "hg-button";
                buttonDiv.setAttribute("data-skbtn", button);
                
                const displayText = this.options.display[button] || button;
                buttonDiv.textContent = displayText;
                
                rowDiv.appendChild(buttonDiv);
            });
            
            container.appendChild(rowDiv);
        });
    }

    bindEvents() {
        const container = document.querySelector(".simple-keyboard");
        if (!container) return;

        container.addEventListener("click", (e) => {
            const button = e.target.closest(".hg-button");
            if (!button) return;

            const buttonValue = button.getAttribute("data-skbtn");
            this.handleButtonPress(buttonValue);
        });
    }

    handleButtonPress(button) {
        if (button === "{bksp}") {
            this.input = this.input.slice(0, -1);
        } else if (button === "{enter}") {
            // 处理回车键
        } else if (button === "{shift}" || button === "{lock}") {
            // 处理shift键
        } else {
            // 普通按键
            this.input += button;
        }

        this.options.onKeyPress(button);
        this.options.onChange(this.input);
    }

    setInput(input) {
        this.input = input;
        this.options.onChange(this.input);
    }

    getInput() {
        return this.input;
    }

    clearInput() {
        this.input = "";
        this.options.onChange(this.input);
    }
}

// 全局变量，供其他脚本使用
window.SimpleKeyboard = SimpleKeyboard;
