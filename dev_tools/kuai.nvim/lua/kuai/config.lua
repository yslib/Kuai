local M = {}

M.defaults = {
    repl_cmd = "cargo run --bin kuai --quiet",
}

M.options = {}

function M.set_options(opts)
    M.options = vim.tbl_deep_extend("force", M.defaults, opts or {})
end

return M
