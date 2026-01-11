-- please set vim.o.exrc = true in your main nvim config to load this file

-- local function get_script_dir()
--     local str = debug.getinfo(1).source:sub(2)
--     return vim.fn.fnamemodify(str, ":p:h")
-- end

local project_root = vim.fn.getcwd()
local plugin_path = project_root .. "/dev_tools/tofu.nvim"

vim.notify("Welcome to Tofu Development!\nAdding tofu.nvim to runtime path: " .. plugin_path, vim.log.levels.INFO,
    { title = "Tofu Dev" })
vim.opt.rtp:prepend(plugin_path)

require("tofu").setup({
    repl_cmd = "cargo run --bin tofu -- -l",
})
