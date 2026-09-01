-- please set vim.o.exrc = true in your main nvim config to load this file

-- local function get_script_dir()
--     local str = debug.getinfo(1).source:sub(2)
--     return vim.fn.fnamemodify(str, ":p:h")
-- end

local project_root = vim.fn.getcwd()
local plugin_path = project_root .. "/dev_tools/kuai.nvim"

vim.notify("Welcome to Kuai Development!\nAdding kuai.nvim to runtime path: " .. plugin_path, vim.log.levels.INFO,
    { title = "Kuai Dev" })
vim.opt.rtp:prepend(plugin_path)

require("kuai").setup({
    repl_cmd = "cargo run --bin kuai -- -l",
})
