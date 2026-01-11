-- need toggleterm plugin
local toggleterm = require("toggleterm")
local config = require("tofu.config")

local tofu_repl = require("toggleterm.terminal").Terminal:new({
    cmd = "cargo run --bin tofu -- -l",
    direction = "vertical",
    close_on_exit = true,
    hidden = true,
    on_open = function(term)
        vim.api.nvim_buf_set_keymap(term.bufnr, "t", "<esc>", [[<C-\><C-n>]], { noremap = true, silent = true })
    end,
})

local function send_to_rust_server(ip, port, msg)
    local uv = vim.uv
    local client = uv.new_tcp()

    if client then
        client:connect(ip, port, function(err)
            if err then
                print("connect failed: ", err)
                client:close()
                return
            end

            -- 1. 发送消息
            client:write(msg, function(err)
                if err then
                    print("write error: ", err)
                    client:close()
                else
                    -- 2. 关键步骤：发送 Shutdown (FIN)
                    -- 这告诉 Rust 的 read_to_string "由于 EOF，读取结束"
                    -- 但不会产生 RST 错误
                    client:shutdown(function()
                        -- 3. 确认对方收到 FIN 后，再关闭句柄
                        client:close()
                        vim.schedule(function()
                        end)
                    end)
                end
            end)
        end)
    end
end
local function get_visual_selection()
    -- 1. 获取当前模式，如果是在 Visual 模式下，需要先退出才能更新标记
    local mode = vim.fn.mode()
    if mode == 'v' or mode == 'V' or mode == '\22' then -- \22 是 CTRL-V
        vim.cmd('normal! \27')                          -- 发送 ESC，退出 Visual 模式，更新 '< 和 '> 标记
    end

    -- 2. 保存当前寄存器内容（防止污染用户的剪贴板）
    local save_reg = vim.fn.getreg('"')
    local save_regtype = vim.fn.getregtype('"')

    -- 3. 重新选中上一次的选区，并“复制”到未命名寄存器
    -- gv: 重新选中上一次选区
    -- y: 复制
    vim.cmd('normal! gvy')

    -- 4. 从寄存器获取内容
    local selection = vim.fn.getreg('"')

    -- 5. 还原寄存器
    vim.fn.setreg('"', save_reg, save_regtype)

    return selection
end

local function send_to_dsl_repl()
    if not tofu_repl:is_open() then
        tofu_repl:open()
    end
    local mode = vim.api.nvim_get_mode().mode
    local text = "";
    if mode == 'v' or mode == 'V' or mode == '\22' then -- \22 是 CTRL-V
        text = get_visual_selection()
    else
        -- not in visual mode, send the whole buffer
        local all_lines = vim.api.nvim_buf_get_lines(0, 0, -1, false)
        text = table.concat(all_lines, "\n")
    end
    vim.notify("Sending to Tofu REPL:\n" .. text, vim.log.levels.INFO, { title = "Tofu Dev" })
    send_to_rust_server("127.0.0.1", 9999, text)
    -- return to previous window
end


-- keymap
vim.keymap.set({ 'n', 'v' }, "<C-e>", function()
    send_to_dsl_repl()
end, { desc = "Send selection to Tofu REPL" })
--
-- vim.keymap.set("n", "<leader>re", function()
-- end, { desc = "Send current line to Tofu REPL" })
