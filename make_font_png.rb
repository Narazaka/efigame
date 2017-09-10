require "pathname"

Pathname.new("fs/fonts").mkpath
Pathname.new("texts").children.each do |text_file|
  cmd = "convert -background none -fill black +antialias -font JF-Dot-Kappa20-0213.ttf -pointsize 20 label:@#{text_file} fs/fonts/#{text_file.basename}.png"
  puts cmd
  system cmd
end
