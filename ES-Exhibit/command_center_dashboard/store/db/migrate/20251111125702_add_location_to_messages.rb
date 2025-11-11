class AddLocationToMessages < ActiveRecord::Migration[8.0]
  def change
    add_column :messages, :location, :string
  end
end
