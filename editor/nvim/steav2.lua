-- steav2: .sts filetype + steav2-lsp. install.sh copies this to
-- <nvim config>/lua/steav2.lua, then add `require("steav2")` to your config
-- (init.lua for Neovim, config.lua for LunarVim).
vim.filetype.add({ extension = { sts = "steav2" } })

vim.api.nvim_create_autocmd("FileType", {
  pattern = "steav2",
  callback = function(args)
    local config = {
      name = "steav2-lsp",
      cmd = { "steav2-lsp" },
      root_dir = vim.fs.root(args.buf, { ".git" }) or vim.fn.getcwd(),
    }
    -- LunarVim's keymaps + completion capabilities, when it's there
    local ok, lvim_lsp = pcall(require, "lvim.lsp")
    if ok then
      config.on_attach = lvim_lsp.common_on_attach
      config.capabilities = lvim_lsp.common_capabilities()
    end
    vim.lsp.start(config)
  end,
})
