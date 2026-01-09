-- need toggleterm plugin
local toggleterm = require("toggleterm")
local config = require("tofu.config")

local tofu_repl = require("toggleterm.terminal").Terminal:new({
    cmd = config.options.repl_cmd,
    direction = "vertical",
    close_on_exit = true,
    hidden = true,
    on_open = function(term)
        vim.api.nvim_buf_set_keymap(term.bufnr, "t", "<esc>", [[<C-\><C-n>]], { noremap = true, silent = true })
    end,
})

local function send_to_dsl_repl()
    if not tofu_repl:is_open() then
        tofu_repl:open()
    end
    local mode = vim.api.nvim_get_mode().mode
    if mode == 'v' or mode == 'V' or mode == "\22" then
        -- visual mode
        local trim_spaces = true
        require("toggleterm").send_lines_to_terminal("visual_selection", trim_spaces, { args = tofu_repl.id })
        tofu_repl:send("\r")
    else
        local all_lines = vim.api.nvim_buf_get_lines(0, 0, -1, false)
        local text = table.concat(all_lines, "\n")
        vim.notify(text)
        tofu_repl:send(text)
        tofu_repl:send("\r")
    end
    -- return to previous window
    vim.cmd("wincmd p")
end

-- keymap
vim.keymap.set({ 'n', 'v' }, "<C-e>", function()
    send_to_dsl_repl()
end, { desc = "Send selection to Tofu REPL" })

vim.keymap.set("n", "<leader>re", function()
end, { desc = "Send current line to Tofu REPL" })
