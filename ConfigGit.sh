
# configure git behaviors and colorizations:
git config --global core.editor "nano"
git config --global push.default simple
git config --global pull.rebase true
git config --global alias.hist 'log --pretty=format:"%h %ad | %s%d [%an]" --graph --date=short'
git config --global user.name "Wayne Ross"
git config --global user.email "mr_trekkie@yahoo.com"

#  color list from http://shallowsky.com/blog/programming/gitcolors.html
git config --global color.interactive.prompt 'red bold'
git config --global color.diff.old 'red bold'
git config --global color.diff.new 'green bold'
git config --global color.diff.frag 'cyan bold'
git config --global color.status.added 'red bold'
git config --global color.status.updated 'green bold'
git config --global color.status.changed 'red bold'
git config --global color.status.untracked 'red bold'
git config --global color.status.nobranch 'red bold'

